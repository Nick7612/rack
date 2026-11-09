import { Game, PHASE, RULES, rulesName } from './game.js';
import { TableView } from './render.js';
import { Sound } from './audio.js';

const $ = (id) => document.getElementById(id);

const homeScreen = $('home-screen');
const gameScreen = $('game-screen');
const canvas = $('table');
const winnerPanel = $('winner');
const rulesDialog = $('rules-dialog');

const view = new TableView(canvas);
const sound = new Sound();

let game = null;
let charging = false;
let press = { x: 0, y: 0 };
let hudDirty = true;
let lastTime = performance.now();
let accumulator = 0;
let muted = localStorage.getItem('rack.muted') === '1';

sound.setMuted(muted);

function noteSoundButton() {
    const label = muted ? 'Sound: off' : 'Sound: on';
    $('btn-sound').textContent = label;
    $('btn-game-sound').textContent = muted ? 'Sound off' : 'Sound on';
}

function setMuted(value) {
    muted = value;
    sound.setMuted(muted);
    localStorage.setItem('rack.muted', muted ? '1' : '0');
    noteSoundButton();
    if (!muted)
        sound.unlock();
}

function showWinner(winner, reason) {
    if (!game)
        return;
    $('winner-name').textContent = winner >= 0 ? `${game.names[winner]} wins` : 'Match over';
    $('winner-reason').textContent = reason;
    $('winner-kicker').textContent = rulesName(game.rules);
    winnerPanel.hidden = false;
}

function setupHandlers(instance) {
    instance.handlers.change = () => { hudDirty = true; };
    instance.handlers.message = () => { hudDirty = true; };
    instance.handlers.sound = (kind) => sound.play(kind);
    instance.handlers.end = (winner, reason) => showWinner(winner, reason);
}

function startGame(mode, rules, options = {}) {
    sound.unlock();
    game = new Game();
    setupHandlers(game);

    if (mode === 'practice')
        game.startPractice(rules, 'You');
    else
        game.start(rules, options.p1 || 'Player 1', options.p2 || 'Player 2');

    charging = false;
    accumulator = 0;
    winnerPanel.hidden = true;
    homeScreen.hidden = true;
    gameScreen.hidden = false;
    requestAnimationFrame(() => {
        view.resize();
        hudDirty = true;
    });
}

function leaveGame() {
    game = null;
    charging = false;
    winnerPanel.hidden = true;
    gameScreen.hidden = true;
    homeScreen.hidden = false;
}

function updateHud() {
    if (!game)
        return;

    const g = game;
    $('rule-label').textContent = rulesName(g.rules);
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

    if (g.phase === PHASE.BALL_IN_HAND) {
        $('status-hint').textContent = g.ballInHand()
            ? 'Ball in hand — click a legal spot to place the cue ball.'
            : 'Ball in hand.';
    } else if (g.phase === PHASE.ROLLING) {
        $('status-hint').textContent = 'Balls rolling…';
    } else if (g.winner >= 0) {
        $('status-hint').textContent = 'Match over.';
    } else {
        $('status-hint').textContent = '';
    }

    $('power-fill').style.width = `${Math.round(g.aimPower * 100)}%`;
    $('power-value').textContent = `${Math.round(g.aimPower * 100)}%`;
    $('power-fill').classList.toggle('high', g.aimPower > 0.75);

    $('game-hint').textContent = g.phase === PHASE.BALL_IN_HAND
        ? 'Click a legal spot to place the cue ball'
        : (g.myPlayer >= 0 && g.current !== g.myPlayer)
            ? 'Waiting for the other player…'
            : 'Move to aim · drag to set power · release to shoot';
}

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

canvas.addEventListener('pointerdown', (event) => {
    if (!game)
        return;
    sound.unlock();

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
        if (game.ballInHand() && game.canPlaceCueBall(point.x, point.y))
            game.placeCueBall(point.x, point.y);
        return;
    }

    if (charging) {
        charging = false;
        // A tap only aims; a real drag is needed to play a shot. This keeps
        // touch play from firing the cue on every tap.
        const distance = Math.hypot(event.clientX - press.x, event.clientY - press.y);
        if (distance >= 6)
            game.shoot(game.aimAngle, game.aimPower);
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
        if (game.phase !== PHASE.AIMING)
            return;
        event.preventDefault();
        const step = event.shiftKey ? 0.004 : 0.025;
        game.setAim(game.aimAngle + (event.key === 'ArrowLeft' ? -step : step));
        return;
    }

    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
        if (game.phase !== PHASE.AIMING)
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
        game.shoot(game.aimAngle, game.aimPower);
        game.setAimPower(0.55);
    }
});

function frame(now) {
    requestAnimationFrame(frame);
    const delta = Math.min(100, now - lastTime);
    lastTime = now;

    if (!game || gameScreen.hidden)
        return;

    if (game.phase === PHASE.ROLLING) {
        accumulator += delta;
        let steps = 0;
        while (accumulator >= 1000 / 60 && steps < 8) {
            game.advanceFrame();
            accumulator -= 1000 / 60;
            steps += 1;
        }
        if (steps === 8)
            accumulator = 0;
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

for (const button of document.querySelectorAll('[data-start]')) {
    button.addEventListener('click', () => {
        startGame(button.dataset.start, Number(button.dataset.rules));
    });
}

$('btn-rules').addEventListener('click', () => rulesDialog.showModal());
$('btn-game-rules').addEventListener('click', () => rulesDialog.showModal());
$('btn-sound').addEventListener('click', () => setMuted(!muted));
$('btn-game-sound').addEventListener('click', () => setMuted(!muted));
$('btn-leave').addEventListener('click', leaveGame);
$('btn-home').addEventListener('click', leaveGame);
$('btn-rematch').addEventListener('click', () => {
    if (!game)
        return;
    game.rackAgain();
    winnerPanel.hidden = true;
    hudDirty = true;
});

document.addEventListener('visibilitychange', () => {
    if (!document.hidden)
        lastTime = performance.now();
});

noteSoundButton();

// Offline play. Local development skips the worker so assets are never stale.
if ('serviceWorker' in navigator
    && location.protocol !== 'file:'
    && location.hostname !== '127.0.0.1'
    && location.hostname !== 'localhost') {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('sw.js').catch(() => {});
    });
}

// A small hook for screenshots and manual debugging.
window.rack = {
    startGame,
    get game() { return game; },
    view,
    rules: RULES,
    phase: PHASE,
};

// ?play=8 or ?play=9 opens a match straight away; ?practice=1 makes it solo.
const params = new URLSearchParams(window.location.search);
if (params.has('play') || params.has('practice')) {
    const code = params.get('play') || params.get('practice');
    const rules = code === '9' ? RULES.NINE_BALL : RULES.EIGHT_BALL;
    startGame(params.has('practice') ? 'practice' : 'match', rules);

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
