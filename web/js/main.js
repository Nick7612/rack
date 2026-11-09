import { Game, PHASE, RULES, rulesName } from './game.js';
import { TableView } from './render.js';
import { Sound } from './audio.js';
import { Jazz } from './music.js';
import { OnlineSession } from './net.js';
import {
    randomRoomCode, normalizeRoomCode, isValidRoomCode, formatRoomCode,
} from './room.js';

const $ = (id) => document.getElementById(id);

const homeScreen = $('home-screen');
const gameScreen = $('game-screen');
const canvas = $('table');
const winnerPanel = $('winner');
const rulesDialog = $('rules-dialog');
const onlineDialog = $('online-dialog');
const onlineSetup = $('online-setup');
const onlineWait = $('online-wait');
const roomCodeLabel = $('room-code');
const onlineStatusLabel = $('online-status');
const waitStatusLabel = $('wait-status');
const onlineNameInput = $('online-name');
const joinCodeInput = $('join-code');
const nameInputs = [$('name-input-0'), $('name-input-1')];

const view = new TableView(canvas);
const sound = new Sound();
const jazz = new Jazz();

let game = null;
let charging = false;
let press = { x: 0, y: 0 };
let hudDirty = true;
let lastTime = performance.now();
let accumulator = 0;
let muted = localStorage.getItem('rack.muted') === '1';
let musicOn = localStorage.getItem('rack.music') !== '0';
let session = null;
let onlineRole = null;
let onlineCode = '';
let onlineRules = RULES.EIGHT_BALL;
let hostAttempts = 0;
let shotPending = false;

sound.setMuted(muted);
jazz.enabled = musicOn;

/* ---------- Audio ---------- */

function updateAudioButtons() {
    $('btn-sound').textContent = muted ? 'Sound: off' : 'Sound: on';
    $('btn-game-sound').textContent = muted ? 'Sound off' : 'Sound on';
    $('btn-music').textContent = musicOn ? 'Music: on' : 'Music: off';
    $('btn-game-music').textContent = musicOn ? 'Music on' : 'Music off';
}

function unlockAudio() {
    sound.unlock();
    if (musicOn)
        jazz.unlock();
}

function setMuted(value) {
    muted = value;
    sound.setMuted(muted);
    localStorage.setItem('rack.muted', muted ? '1' : '0');
    updateAudioButtons();
    if (!muted)
        sound.unlock();
}

function setMusic(value) {
    musicOn = value;
    localStorage.setItem('rack.music', musicOn ? '1' : '0');
    jazz.setEnabled(musicOn);
    if (musicOn)
        jazz.unlock();
    updateAudioButtons();
}

document.addEventListener('pointerdown', unlockAudio, { once: true });
document.addEventListener('keydown', unlockAudio, { once: true });

/* ---------- Names ---------- */

function defaultName(index) {
    return `Player ${index + 1}`;
}

function savedName(index) {
    const value = localStorage.getItem(`rack.name.${index}`);
    return value && value.trim() ? value.trim().slice(0, 20) : defaultName(index);
}

nameInputs.forEach((input, index) => {
    input.value = savedName(index);
    input.addEventListener('input', () => {
        localStorage.setItem(`rack.name.${index}`, input.value.slice(0, 20));
    });
});

function playerName(index) {
    const value = (nameInputs[index].value || '').trim().slice(0, 20);
    return value || defaultName(index);
}

const savedOnlineName = (localStorage.getItem('rack.onlineName') || '').trim();
onlineNameInput.value = savedOnlineName || playerName(0);
onlineNameInput.addEventListener('input', () => {
    localStorage.setItem('rack.onlineName', onlineNameInput.value.slice(0, 20));
});

function onlineDisplayName() {
    const value = (onlineNameInput.value || '').trim().slice(0, 20);
    return value || playerName(0);
}

/* ---------- Screens ---------- */

function showWinner(winner, reason) {
    if (!game)
        return;
    $('winner-name').textContent = winner >= 0 ? `${game.names[winner]} wins` : 'Match over';
    $('winner-reason').textContent = reason;
    let kicker = rulesName(game.rules);
    if (onlineRole && onlineCode)
        kicker += ` · ${formatRoomCode(onlineCode)}`;
    $('winner-kicker').textContent = kicker;
    $('btn-rematch').hidden = false;
    winnerPanel.hidden = false;
}

function showNotice(title, reason) {
    $('winner-kicker').textContent = 'Online room';
    $('winner-name').textContent = title;
    $('winner-reason').textContent = reason;
    $('btn-rematch').hidden = true;
    winnerPanel.hidden = false;
}

function setupHandlers(instance) {
    instance.handlers.change = () => { hudDirty = true; };
    instance.handlers.message = () => { hudDirty = true; };
    instance.handlers.sound = (kind) => sound.play(kind);
    instance.handlers.end = (winner, reason) => showWinner(winner, reason);
}

function showGameScreen() {
    charging = false;
    accumulator = 0;
    winnerPanel.hidden = true;
    $('btn-rematch').hidden = false;
    homeScreen.hidden = true;
    gameScreen.hidden = false;
    requestAnimationFrame(() => {
        view.resize();
        hudDirty = true;
    });
}

function startGame(mode, rules, options = {}) {
    unlockAudio();

    if (!options.online) {
        if (session) {
            const old = session;
            session = null;
            old.close();
        }
        onlineRole = null;
        onlineCode = '';
    }

    game = new Game();
    setupHandlers(game);

    if (mode === 'practice')
        game.startPractice(rules, options.p1 || 'You');
    else
        game.start(rules, options.p1 || playerName(0), options.p2 || playerName(1));

    if (Number.isInteger(options.myPlayer))
        game.setMyPlayer(options.myPlayer);

    shotPending = false;
    showGameScreen();
}

function startOnlineHost(guestName) {
    if (!session || onlineRole !== 'host')
        return;

    const hostName = onlineDisplayName();
    const guest = (guestName || '').trim().slice(0, 20) || 'Player 2';

    startGame('online', onlineRules, {
        p1: hostName,
        p2: guest,
        myPlayer: 0,
        online: true,
    });

    session.sendWelcome(game.snapshot());
    closeOnlineDialog();
    setStatus(onlineStatusLabel, '');
}

function startOnlineGuest(snapshot) {
    unlockAudio();

    game = new Game();
    setupHandlers(game);
    game.applySnapshot(snapshot);
    game.setMyPlayer(1);
    game.practice = false;
    shotPending = false;
    showGameScreen();

    if (snapshot.winner >= 0)
        showWinner(snapshot.winner, snapshot.message);

    closeOnlineDialog();
    hudDirty = true;
}

function leaveGame() {
    if (session) {
        const old = session;
        session = null;
        old.close();
    }
    onlineRole = null;
    onlineCode = '';
    hostAttempts = 0;
    shotPending = false;
    charging = false;
    game = null;
    winnerPanel.hidden = true;
    gameScreen.hidden = true;
    homeScreen.hidden = false;
    showOnlineSetup();
    setStatus(onlineStatusLabel, '');
    hudDirty = true;
    accumulator = 0;
    lastTime = performance.now();
}

function updateHud() {
    if (!game)
        return;

    const g = game;
    let label = rulesName(g.rules);
    if (onlineRole && onlineCode)
        label += ` · ${formatRoomCode(onlineCode)}`;
    $('rule-label').textContent = label;
    $('message').textContent = g.message;

    for (const index of [0, 1]) {
        $(`name-${index}`).textContent = g.names[index];

        let chip = 'Open table';
        if (g.rules === RULES.NINE_BALL)
            chip = '9-ball';
        else if (g.group[index] === 1)
            chip = 'Solids';
        else if (g.group[index] === 2)
            chip = 'Stripes';
        $(`group-${index}`).textContent = chip;

        const active = g.winner < 0 && g.current === index;
        const card = $(`player-card-${index}`);
        card.classList.toggle('active', active);
        card.classList.toggle('winner', g.winner === index);

        let left = '';
        if (g.rules === RULES.NINE_BALL)
            left = `${g.remainingFor(index)} on the table`;
        else if (g.group[index] !== 0)
            left = `${g.remainingFor(index)} left`;
        $(`left-${index}`).textContent = left;
    }

    const myTurn = !onlineRole || g.current === g.myPlayer;
    const currentName = g.names[g.current] || 'the other player';

    if (g.winner >= 0) {
        $('status-hint').textContent = 'Match over.';
    } else if (g.phase === PHASE.BALL_IN_HAND) {
        $('status-hint').textContent = g.ballInHand()
            ? 'Ball in hand — click a legal spot to place the cue ball.'
            : `Ball in hand for ${currentName}.`;
    } else if (g.phase === PHASE.ROLLING) {
        $('status-hint').textContent = onlineRole === 'guest'
            ? 'Following the shot…'
            : 'Balls rolling…';
    } else if (onlineRole && myTurn) {
        $('status-hint').textContent = 'Your shot.';
    } else if (onlineRole) {
        $('status-hint').textContent = `Waiting for ${currentName}…`;
    } else {
        $('status-hint').textContent = '';
    }

    $('power-fill').style.width = `${Math.round(g.aimPower * 100)}%`;
    $('power-value').textContent = `${Math.round(g.aimPower * 100)}%`;
    $('power-fill').classList.toggle('high', g.aimPower > 0.75);

    $('game-hint').textContent = g.phase === PHASE.BALL_IN_HAND
        ? (g.ballInHand() ? 'Click a legal spot to place the cue ball' : `Waiting for ${currentName}…`)
        : shotPending
            ? 'Shot sent…'
            : !myTurn
                ? `Waiting for ${currentName}…`
                : 'Move to aim · drag to set power · release to shoot';
}

/* ---------- Aiming and shooting ---------- */

function currentCue() {
    return game ? game.physics.find(0) : null;
}

function aimAt(clientX, clientY) {
    const cue = currentCue();
    if (!cue || !cue.active || !game)
        return;
    const point = view.toTable(clientX, clientY);
    game.setAim(Math.atan2(point.y - cue.y, point.x - cue.x));
}

function fireShot(angle, power) {
    if (!game || shotPending)
        return;

    if (onlineRole === 'guest') {
        if (!session)
            return;
        shotPending = true;
        session.sendShot(angle, power);
        game.setAimPower(0.55);
        hudDirty = true;
        return;
    }

    if (!game.shoot(angle, power))
        return;
    game.setAimPower(0.55);

    if (onlineRole === 'host' && session)
        session.sendState(game.snapshot(), true);
}

canvas.addEventListener('pointerdown', (event) => {
    if (!game || shotPending)
        return;
    unlockAudio();

    const point = view.toTable(event.clientX, event.clientY);
    if (game.phase === PHASE.BALL_IN_HAND) {
        if (game.ballInHand() && game.canPlaceCueBall(point.x, point.y))
            game.previewCueBall(point.x, point.y);
        return;
    }
    if (game.phase !== PHASE.AIMING || !game.canShoot())
        return;

    canvas.setPointerCapture(event.pointerId);
    press = { x: event.clientX, y: event.clientY };
    aimAt(event.clientX, event.clientY);
    game.setAimPower(0.1);
    charging = true;
    event.preventDefault();
}, { passive: false });

canvas.addEventListener('pointermove', (event) => {
    if (!game)
        return;

    const point = view.toTable(event.clientX, event.clientY);
    if (game.phase === PHASE.BALL_IN_HAND) {
        if (game.ballInHand())
            game.previewCueBall(point.x, point.y);
        return;
    }

    if (charging) {
        const distance = Math.hypot(event.clientX - press.x, event.clientY - press.y);
        game.setAimPower(Math.max(0.08, Math.min(1, distance / 240)));
        return;
    }

    if (game.phase === PHASE.AIMING && game.canShoot())
        aimAt(event.clientX, event.clientY);
});

canvas.addEventListener('pointerup', (event) => {
    if (!game)
        return;

    if (game.phase === PHASE.BALL_IN_HAND) {
        const point = view.toTable(event.clientX, event.clientY);
        if (game.ballInHand() && game.canPlaceCueBall(point.x, point.y)) {
            if (onlineRole === 'guest') {
                game.setMessage('Placing the cue ball…');
                if (session)
                    session.sendPlace(point.x, point.y);
            } else {
                game.placeCueBall(point.x, point.y);
                if (onlineRole === 'host' && session)
                    session.sendState(game.snapshot(), true);
            }
        }
        return;
    }

    if (charging) {
        charging = false;
        // A tap only aims; a real drag is needed to play a shot. This keeps
        // touch play from firing the cue on every tap.
        const distance = Math.hypot(event.clientX - press.x, event.clientY - press.y);
        if (distance >= 6)
            fireShot(game.aimAngle, game.aimPower);
        else
            game.setAimPower(0.55);
    }
});

canvas.addEventListener('pointercancel', () => {
    charging = false;
});

canvas.addEventListener('contextmenu', (event) => event.preventDefault());

window.addEventListener('keydown', (event) => {
    if (!game || gameScreen.hidden)
        return;
    if (event.target instanceof HTMLInputElement || event.target instanceof HTMLTextAreaElement)
        return;

    if (event.key === 'Escape') {
        leaveGame();
        return;
    }

    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
        if (game.phase !== PHASE.AIMING || !game.canShoot())
            return;
        event.preventDefault();
        const step = event.shiftKey ? 0.004 : 0.025;
        game.setAim(game.aimAngle + (event.key === 'ArrowLeft' ? -step : step));
        return;
    }

    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
        if (game.phase !== PHASE.AIMING || !game.canShoot())
            return;
        event.preventDefault();
        const step = event.shiftKey ? 0.01 : 0.05;
        game.setAimPower(game.aimPower + (event.key === 'ArrowUp' ? step : -step));
        return;
    }

    if (event.code === 'Space') {
        event.preventDefault();
        if (event.repeat || !game.canShoot())
            return;
        fireShot(game.aimAngle, game.aimPower);
    }
});

/* ---------- Online rooms ---------- */

function setStatus(element, text, error = false) {
    element.textContent = text;
    element.classList.toggle('error', error);
}

function setBusy(busy) {
    $('btn-host').disabled = busy;
    $('btn-join').disabled = busy;
    onlineNameInput.disabled = busy;
    joinCodeInput.disabled = busy;
}

function showOnlineSetup() {
    onlineSetup.hidden = false;
    onlineWait.hidden = true;
    setBusy(false);
}

function showOnlineWait(code) {
    onlineSetup.hidden = true;
    onlineWait.hidden = false;
    roomCodeLabel.textContent = formatRoomCode(code);
    $('btn-copy-code').textContent = 'Copy code';
    setStatus(waitStatusLabel, 'Waiting for the other player…');
}

function openOnlineDialog() {
    if (!onlineNameInput.value.trim())
        onlineNameInput.value = playerName(0);
    showOnlineSetup();
    setStatus(onlineStatusLabel, '');
    if (!onlineDialog.open)
        onlineDialog.showModal();
}

function closeOnlineDialog() {
    if (onlineDialog.open)
        onlineDialog.close();
}

function selectOnlineRules(value) {
    onlineRules = value === RULES.NINE_BALL ? RULES.NINE_BALL : RULES.EIGHT_BALL;
    for (const button of onlineDialog.querySelectorAll('[data-online-rules]'))
        button.classList.toggle('active', Number(button.dataset.onlineRules) === onlineRules);
}

function attachSession() {
    if (session) {
        const old = session;
        session = null;
        old.close(false);
    }

    const current = new OnlineSession();
    session = current;

    current.handlers.status = (status) => {
        if (onlineRole === 'host' && status === 'connected' && !onlineWait.hidden)
            setStatus(waitStatusLabel, 'Player connected. Starting the table…');
        else if (onlineRole === 'guest' && status === 'connected')
            setStatus(onlineStatusLabel, 'Connected. Waiting for the host to start…');
    };

    current.handlers.error = (message) => {
        if (session === current)
            handleOnlineFailure(message);
    };

    current.handlers.roomTaken = () => {
        if (session === current)
            handleRoomTaken();
    };

    current.handlers.roomNotFound = () => {
        if (session !== current)
            return;
        session = null;
        onlineRole = null;
        onlineCode = '';
        showOnlineSetup();
        setStatus(onlineStatusLabel, 'That room was not found. Check the code and try again.', true);
    };

    current.handlers.hello = (guestName) => {
        if (session === current && onlineRole === 'host')
            startOnlineHost(guestName);
    };

    current.handlers.welcome = (snapshot) => {
        if (session === current && onlineRole === 'guest')
            startOnlineGuest(snapshot);
    };

    current.handlers.state = (snapshot) => {
        if (session === current && onlineRole === 'guest')
            applyRemoteState(snapshot);
    };

    current.handlers.shot = (angle, power) => {
        if (session !== current || onlineRole !== 'host' || !game)
            return;
        if (game.phase !== PHASE.AIMING || game.current !== 1) {
            current.sendState(game.snapshot(), true);
            return;
        }
        if (game.shoot(angle, power)) {
            game.setAimPower(0.55);
            current.sendState(game.snapshot(), true);
        }
    };

    current.handlers.place = (x, y) => {
        if (session !== current || onlineRole !== 'host' || !game)
            return;
        if (game.phase === PHASE.BALL_IN_HAND && game.current === 1)
            game.placeCueBall(x, y);
        current.sendState(game.snapshot(), true);
    };

    current.handlers.rematch = () => {
        if (session !== current || onlineRole !== 'host' || !game)
            return;
        game.rackAgain();
        winnerPanel.hidden = true;
        current.sendState(game.snapshot(), true);
        hudDirty = true;
    };

    current.handlers.left = () => {
        if (session === current)
            handleOpponentLeft();
    };
}

function applyRemoteState(snapshot) {
    if (!game || onlineRole !== 'guest')
        return;

    shotPending = false;
    game.applySnapshot(snapshot);
    game.setMyPlayer(1);
    game.practice = false;

    if (snapshot.winner >= 0) {
        showWinner(snapshot.winner, snapshot.message);
    } else if (!winnerPanel.hidden) {
        winnerPanel.hidden = true;
        $('btn-rematch').hidden = false;
    }
    hudDirty = true;
}

function handleRoomTaken() {
    if (onlineRole !== 'host')
        return;

    hostAttempts += 1;
    if (hostAttempts > 4) {
        handleOnlineFailure('Could not find a free room code. Please try again.');
        return;
    }

    if (session) {
        const old = session;
        session = null;
        old.close(false);
    }

    onlineCode = randomRoomCode();
    showOnlineWait(onlineCode);
    attachSession();
    session.host({
        code: onlineCode,
        name: onlineDisplayName(),
        rules: onlineRules,
    });
}

function handleOnlineFailure(message) {
    if (session) {
        const old = session;
        session = null;
        old.close(false);
    }

    const hadGame = Boolean(game && !gameScreen.hidden);
    onlineRole = null;
    onlineCode = '';

    if (hadGame && game) {
        game.phase = PHASE.FINISHED;
        game.winner = -1;
        game.setMessage(message);
        game.emitChanged();
        showNotice('Connection lost', message);
        hudDirty = true;
    } else {
        showOnlineSetup();
        setStatus(onlineStatusLabel, message, true);
        if (!onlineDialog.open)
            onlineDialog.showModal();
    }
}

function handleOpponentLeft() {
    if (session) {
        const old = session;
        session = null;
        old.close(false);
    }

    const hadGame = Boolean(game && !gameScreen.hidden);
    onlineRole = null;
    onlineCode = '';

    if (hadGame && game) {
        game.phase = PHASE.FINISHED;
        game.winner = -1;
        game.setMessage('The other player left the room.');
        game.emitChanged();
        showNotice('Opponent left', 'The room closed. Head home to start another table.');
        hudDirty = true;
    } else {
        showOnlineSetup();
        setStatus(onlineStatusLabel, 'The other player left before the match started.', true);
    }
}

function cancelRoom() {
    if (session) {
        const old = session;
        session = null;
        old.close();
    }
    onlineRole = null;
    onlineCode = '';
    hostAttempts = 0;
    showOnlineSetup();
    setStatus(onlineStatusLabel, 'Room closed.');
}

function hostRoom() {
    if (typeof window.Peer !== 'function') {
        setStatus(onlineStatusLabel, 'Online play needs an internet connection the first time it loads.', true);
        return;
    }

    const name = onlineDisplayName();
    localStorage.setItem('rack.onlineName', name);

    onlineRole = 'host';
    onlineCode = randomRoomCode();
    hostAttempts = 0;
    showOnlineWait(onlineCode);
    attachSession();
    session.host({ code: onlineCode, name, rules: onlineRules });
}

function joinRoom() {
    if (typeof window.Peer !== 'function') {
        setStatus(onlineStatusLabel, 'Online play needs an internet connection the first time it loads.', true);
        return;
    }

    const code = normalizeRoomCode(joinCodeInput.value);
    if (!isValidRoomCode(code)) {
        setStatus(onlineStatusLabel, 'Enter the six-character room code.', true);
        joinCodeInput.focus();
        return;
    }

    const name = onlineDisplayName();
    localStorage.setItem('rack.onlineName', name);

    onlineRole = 'guest';
    onlineCode = code;
    showOnlineSetup();
    setBusy(true);
    setStatus(onlineStatusLabel, `Connecting to room ${formatRoomCode(code)}…`);
    attachSession();
    session.join({ code, name });
}

async function copyRoomCode() {
    if (!onlineCode)
        return;

    let copied = false;
    try {
        await navigator.clipboard.writeText(onlineCode);
        copied = true;
    } catch {
        const area = document.createElement('textarea');
        area.value = onlineCode;
        area.style.position = 'fixed';
        area.style.opacity = '0';
        document.body.appendChild(area);
        area.select();
        try {
            copied = document.execCommand('copy');
        } catch {
            copied = false;
        }
        area.remove();
    }

    $('btn-copy-code').textContent = copied ? 'Copied!' : 'Select the code above';
    setTimeout(() => {
        if (!onlineWait.hidden)
            $('btn-copy-code').textContent = 'Copy code';
    }, 1600);
}

/* ---------- Frame loop ---------- */

function frame(now) {
    requestAnimationFrame(frame);
    const delta = Math.min(100, now - lastTime);
    lastTime = now;

    if (!game || gameScreen.hidden)
        return;

    if (game.phase === PHASE.ROLLING) {
        if (onlineRole === 'guest') {
            // The host owns the table while online. The snapshots it sends
            // are the only source of ball motion here.
            accumulator = 0;
        } else {
            accumulator += delta;
            let steps = 0;
            while (accumulator >= 1000 / 60 && steps < 8) {
                game.advanceFrame();
                accumulator -= 1000 / 60;
                steps += 1;
            }
            if (steps === 8)
                accumulator = 0;

            if (onlineRole === 'host' && session) {
                if (game.phase !== PHASE.ROLLING)
                    session.sendState(game.snapshot(), true);
                else
                    session.sendState(game.snapshot());
            }
        }
    } else {
        accumulator = 0;
    }

    view.draw(game);
    if (hudDirty) {
        updateHud();
        hudDirty = false;
    }
}

new ResizeObserver(() => {
    view.resize();
    hudDirty = true;
}).observe(canvas);

/* ---------- Controls ---------- */

for (const button of document.querySelectorAll('[data-start]')) {
    button.addEventListener('click', () => {
        startGame(button.dataset.start, Number(button.dataset.rules));
    });
}

for (const button of onlineDialog.querySelectorAll('[data-online-rules]')) {
    button.addEventListener('click', () => selectOnlineRules(Number(button.dataset.onlineRules)));
}
selectOnlineRules(RULES.EIGHT_BALL);

$('btn-rules').addEventListener('click', () => rulesDialog.showModal());
$('btn-game-rules').addEventListener('click', () => rulesDialog.showModal());
$('btn-sound').addEventListener('click', () => setMuted(!muted));
$('btn-game-sound').addEventListener('click', () => setMuted(!muted));
$('btn-music').addEventListener('click', () => setMusic(!musicOn));
$('btn-game-music').addEventListener('click', () => setMusic(!musicOn));
$('btn-leave').addEventListener('click', leaveGame);
$('btn-home').addEventListener('click', leaveGame);
$('btn-online').addEventListener('click', openOnlineDialog);
$('btn-host').addEventListener('click', hostRoom);
$('btn-join').addEventListener('click', joinRoom);
$('btn-copy-code').addEventListener('click', copyRoomCode);
$('btn-cancel-room').addEventListener('click', cancelRoom);
$('btn-online-close').addEventListener('click', () => {
    if (onlineRole)
        cancelRoom();
    closeOnlineDialog();
});

joinCodeInput.addEventListener('input', () => {
    const code = normalizeRoomCode(joinCodeInput.value);
    if (joinCodeInput.value !== code)
        joinCodeInput.value = code;
});
joinCodeInput.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') {
        event.preventDefault();
        joinRoom();
    }
});

onlineDialog.addEventListener('cancel', (event) => {
    event.preventDefault();
    if (onlineRole)
        cancelRoom();
    closeOnlineDialog();
});

$('btn-rematch').addEventListener('click', () => {
    if (!game)
        return;

    if (onlineRole === 'guest') {
        if (session)
            session.requestRematch();
        $('winner-reason').textContent = 'Waiting for the host to re-rack…';
        return;
    }

    game.rackAgain();
    winnerPanel.hidden = true;
    hudDirty = true;
    if (onlineRole === 'host' && session)
        session.sendState(game.snapshot(), true);
});

document.addEventListener('visibilitychange', () => {
    if (!document.hidden)
        lastTime = performance.now();
});

updateAudioButtons();

// Offline play. Local development skips the worker so assets are never stale.
if ('serviceWorker' in navigator
    && location.protocol !== 'file:'
    && location.hostname !== '127.0.0.1'
    && location.hostname !== 'localhost') {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('sw.js').catch(() => {});
    });
}

// A small hook for screenshots, tests, and manual debugging.
window.rack = {
    startGame,
    get game() { return game; },
    get session() { return session; },
    get onlineRole() { return onlineRole; },
    get onlineCode() { return onlineCode; },
    view,
    music: jazz,
    sound,
    rules: RULES,
    phase: PHASE,
};

// ?play=8 or ?play=9 opens a match straight away; ?practice=1 makes it solo.
const params = new URLSearchParams(window.location.search);
if (params.has('play') || params.has('practice')) {
    const code = params.get('play') || params.get('practice');
    const rules = code === '9' ? RULES.NINE_BALL : RULES.EIGHT_BALL;
    startGame(params.has('practice') ? 'practice' : 'match', rules);

    const aim = Number(params.get('aim'));
    if (Number.isFinite(aim) && params.has('aim')) {
        setTimeout(() => {
            if (game)
                game.setAim(aim);
        }, 120);
    }

    // ?shot=break is handy for screenshots and manual smoke tests. Add
    // &settle=1 to run the simulation to rest without waiting in real time.
    if (params.get('shot') === 'break') {
        setTimeout(() => {
            if (!game)
                return;
            game.shoot(0, 1);
            if (params.get('settle') === '1') {
                let guard = 0;
                while (game.phase === PHASE.ROLLING && guard < 6000) {
                    game.advanceFrame();
                    guard += 1;
                }
                game.setAimPower(0.55);
            }
        }, 150);
    }
}

requestAnimationFrame(frame);
