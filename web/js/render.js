// Canvas renderer for the table, balls, cue, and aiming guide.

import { TABLE, inPocket } from './physics.js';
import { PHASE, RULES, isStripe } from './game.js';
import { firstHit, contactGuide, reflectedDir } from './aim.js';

const RAIL = 0.115;

const BALL_COLORS = {
    1: '#f2c200', 2: '#1c6dd0', 3: '#d32f2f', 4: '#7b1fa2',
    5: '#ef6c00', 6: '#2e7d32', 7: '#8e2424', 8: '#141414',
    9: '#f2c200', 10: '#1c6dd0', 11: '#d32f2f', 12: '#7b1fa2',
    13: '#ef6c00', 14: '#2e7d32', 15: '#8e2424',
};

const FELT = '#0c6b46';
const FELT_DARK = '#085238';
const WOOD = '#5b3320';
const WOOD_DARK = '#3a1f12';
const WOOD_LIGHT = '#7a4630';

function roundRect(ctx, x, y, w, h, r) {
    const radius = Math.min(r, w / 2, h / 2);
    ctx.beginPath();
    ctx.moveTo(x + radius, y);
    ctx.arcTo(x + w, y, x + w, y + h, radius);
    ctx.arcTo(x + w, y + h, x, y + h, radius);
    ctx.arcTo(x, y + h, x, y, radius);
    ctx.arcTo(x, y, x + w, y, radius);
    ctx.closePath();
}

export class TableView {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.cssWidth = 1;
        this.cssHeight = 1;
        this.dpr = 1;
        this.scale = 1;
        this.ox = 0;
        this.oy = 0;
        this.accent = '#ffd60a';
        this.resize();
    }

    resize() {
        const rect = this.canvas.getBoundingClientRect();
        const width = Math.max(1, rect.width);
        const height = Math.max(1, rect.height);
        const dpr = Math.min(2, window.devicePixelRatio || 1);
        const backingWidth = Math.round(width * dpr);
        const backingHeight = Math.round(height * dpr);

        if (this.canvas.width !== backingWidth || this.canvas.height !== backingHeight) {
            this.canvas.width = backingWidth;
            this.canvas.height = backingHeight;
        }
        this.cssWidth = width;
        this.cssHeight = height;
        this.dpr = dpr;

        const padding = 6;
        const outerWidth = TABLE.width + 2 * RAIL;
        const outerHeight = TABLE.height + 2 * RAIL;
        this.scale = Math.max(0.001, Math.min(
            (width - 2 * padding) / outerWidth,
            (height - 2 * padding) / outerHeight,
        ));
        this.ox = (width - TABLE.width * this.scale) / 2;
        this.oy = (height - TABLE.height * this.scale) / 2;
    }

    // Table coordinates have the origin at the bottom-left, so the y axis
    // flips between the model and the canvas.
    toTable(clientX, clientY) {
        const rect = this.canvas.getBoundingClientRect();
        const px = clientX - rect.left;
        const py = clientY - rect.top;
        return {
            x: (px - this.ox) / this.scale,
            y: TABLE.height - (py - this.oy) / this.scale,
        };
    }

    sx(x) {
        return this.ox + x * this.scale;
    }

    sy(y) {
        return this.oy + (TABLE.height - y) * this.scale;
    }

    draw(game) {
        // The canvas can be shown after the first frame, and ResizeObserver
        // only fires after layout. Re-check here so the first paint is never
        // measured against a hidden, zero-sized canvas.
        const rect = this.canvas.getBoundingClientRect();
        if (Math.abs(rect.width - this.cssWidth) > 0.5
            || Math.abs(rect.height - this.cssHeight) > 0.5)
            this.resize();

        const ctx = this.ctx;
        ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
        ctx.clearRect(0, 0, this.cssWidth, this.cssHeight);

        this.drawRail(ctx);
        this.drawFelt(ctx);
        this.drawPockets(ctx);
        this.drawMarkings(ctx);
        this.drawBalls(ctx, game);
        this.drawAim(ctx, game);
        this.drawCueGhost(ctx, game);
    }

    drawRail(ctx) {
        const s = this.scale;
        const x = this.sx(0) - RAIL * s;
        const y = this.sy(TABLE.height) - RAIL * s;
        const w = (TABLE.width + 2 * RAIL) * s;
        const h = (TABLE.height + 2 * RAIL) * s;

        ctx.save();
        ctx.shadowColor = 'rgba(0, 0, 0, 0.45)';
        ctx.shadowBlur = 18;
        ctx.shadowOffsetY = 8;
        const gradient = ctx.createLinearGradient(x, y, x, y + h);
        gradient.addColorStop(0, WOOD_LIGHT);
        gradient.addColorStop(0.18, WOOD);
        gradient.addColorStop(0.82, WOOD);
        gradient.addColorStop(1, WOOD_DARK);
        ctx.fillStyle = gradient;
        roundRect(ctx, x, y, w, h, 26);
        ctx.fill();
        ctx.restore();

        ctx.save();
        ctx.strokeStyle = 'rgba(0, 0, 0, 0.35)';
        ctx.lineWidth = 1;
        roundRect(ctx, x + 0.5, y + 0.5, w - 1, h - 1, 26);
        ctx.stroke();
        ctx.restore();
    }

    drawFelt(ctx) {
        const s = this.scale;
        const x = this.sx(0);
        const y = this.sy(TABLE.height);
        const w = TABLE.width * s;
        const h = TABLE.height * s;

        const gradient = ctx.createRadialGradient(
            x + w / 2, y + h / 2, Math.min(w, h) * 0.08,
            x + w / 2, y + h / 2, Math.max(w, h) * 0.72,
        );
        gradient.addColorStop(0, '#128257');
        gradient.addColorStop(0.65, FELT);
        gradient.addColorStop(1, FELT_DARK);
        ctx.fillStyle = gradient;
        ctx.fillRect(x, y, w, h);

        ctx.save();
        ctx.strokeStyle = 'rgba(0, 0, 0, 0.4)';
        ctx.lineWidth = Math.max(2, s * 0.012);
        ctx.strokeRect(x, y, w, h);
        ctx.restore();
    }

    drawPockets(ctx) {
        const s = this.scale;
        const pockets = [
            [0, 0, TABLE.cornerPocketRadius],
            [TABLE.width / 2, 0, TABLE.sidePocketRadius],
            [TABLE.width, 0, TABLE.cornerPocketRadius],
            [0, TABLE.height, TABLE.cornerPocketRadius],
            [TABLE.width / 2, TABLE.height, TABLE.sidePocketRadius],
            [TABLE.width, TABLE.height, TABLE.cornerPocketRadius],
        ];

        for (const [x, y, radius] of pockets) {
            const px = this.sx(x);
            const py = this.sy(y);
            const r = radius * s * 1.18;
            ctx.beginPath();
            ctx.arc(px, py, r + Math.max(2, s * 0.006), 0, Math.PI * 2);
            ctx.fillStyle = 'rgba(30, 16, 8, 0.9)';
            ctx.fill();
            ctx.beginPath();
            ctx.arc(px, py, r, 0, Math.PI * 2);
            ctx.fillStyle = '#070707';
            ctx.fill();
            ctx.beginPath();
            ctx.arc(px, py, r * 0.82, 0, Math.PI * 2);
            ctx.fillStyle = '#000';
            ctx.fill();
        }
    }

    drawMarkings(ctx) {
        const s = this.scale;

        // Head string and foot spot.
        ctx.save();
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.10)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 6]);
        const headX = this.sx(TABLE.width * 0.25);
        ctx.beginPath();
        ctx.moveTo(headX, this.sy(0));
        ctx.lineTo(headX, this.sy(TABLE.height));
        ctx.stroke();
        ctx.restore();

        ctx.beginPath();
        ctx.arc(this.sx(TABLE.width * 0.75), this.sy(TABLE.height / 2), Math.max(1.5, s * 0.004), 0, Math.PI * 2);
        ctx.fillStyle = 'rgba(255, 255, 255, 0.35)';
        ctx.fill();

        // Rail diamonds.
        const dot = Math.max(1.5, s * 0.006);
        ctx.fillStyle = 'rgba(255, 232, 190, 0.65)';
        for (let i = 1; i <= 7; i += 1) {
            if (i === 4)
                continue;
            const x = this.sx((TABLE.width * i) / 8);
            for (const y of [this.sy(0) - RAIL * s / 2, this.sy(TABLE.height) + RAIL * s / 2]) {
                ctx.beginPath();
                ctx.arc(x, y, dot, 0, Math.PI * 2);
                ctx.fill();
            }
        }
        for (let i = 1; i <= 3; i += 1) {
            const y = this.sy((TABLE.height * i) / 4);
            for (const x of [this.sx(0) - RAIL * s / 2, this.sx(TABLE.width) + RAIL * s / 2]) {
                ctx.beginPath();
                ctx.arc(x, y, dot, 0, Math.PI * 2);
                ctx.fill();
            }
        }
    }

    drawBalls(ctx, game) {
        const s = this.scale;
        const r = TABLE.ballRadius * s;

        for (const ball of game.physics.balls) {
            if (!ball.active)
                continue;
            const px = this.sx(ball.x);
            const py = this.sy(ball.y);

            // Shadow.
            ctx.beginPath();
            ctx.arc(px + r * 0.18, py + r * 0.22, r, 0, Math.PI * 2);
            ctx.fillStyle = 'rgba(0, 0, 0, 0.35)';
            ctx.fill();

            if (ball.id === 0) {
                const gradient = ctx.createRadialGradient(
                    px - r * 0.35, py - r * 0.4, r * 0.1,
                    px, py, r,
                );
                gradient.addColorStop(0, '#ffffff');
                gradient.addColorStop(0.75, '#f3f1e8');
                gradient.addColorStop(1, '#c9c6b8');
                ctx.beginPath();
                ctx.arc(px, py, r, 0, Math.PI * 2);
                ctx.fillStyle = gradient;
                ctx.fill();
                ctx.strokeStyle = 'rgba(0, 0, 0, 0.35)';
                ctx.lineWidth = 0.8;
                ctx.stroke();
                continue;
            }

            const color = BALL_COLORS[ball.id] || '#888';
            const stripe = isStripe(ball.id);

            ctx.save();
            ctx.beginPath();
            ctx.arc(px, py, r, 0, Math.PI * 2);
            ctx.clip();
            ctx.fillStyle = stripe ? '#f7f3e8' : color;
            ctx.fillRect(px - r, py - r, 2 * r, 2 * r);
            if (stripe) {
                ctx.fillStyle = color;
                ctx.fillRect(px - r, py - r * 0.62, 2 * r, r * 1.24);
            }
            // Top highlight.
            const shine = ctx.createRadialGradient(
                px - r * 0.35, py - r * 0.45, 0,
                px - r * 0.2, py - r * 0.2, r * 1.1,
            );
            shine.addColorStop(0, 'rgba(255, 255, 255, 0.55)');
            shine.addColorStop(0.45, 'rgba(255, 255, 255, 0.06)');
            shine.addColorStop(1, 'rgba(0, 0, 0, 0.18)');
            ctx.fillStyle = shine;
            ctx.fillRect(px - r, py - r, 2 * r, 2 * r);
            ctx.restore();

            ctx.beginPath();
            ctx.arc(px, py, r, 0, Math.PI * 2);
            ctx.strokeStyle = 'rgba(0, 0, 0, 0.4)';
            ctx.lineWidth = 0.8;
            ctx.stroke();

            // Number disc.
            const nr = r * 0.46;
            ctx.beginPath();
            ctx.arc(px, py, nr, 0, Math.PI * 2);
            ctx.fillStyle = '#fbf8ee';
            ctx.fill();
            ctx.fillStyle = '#171717';
            ctx.font = `700 ${Math.max(7, r * 0.72)}px ui-sans-serif, system-ui, sans-serif`;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(String(ball.id), px, py + r * 0.03);
        }
    }

    guideLine(ctx, x0, y0, x1, y1, from, to, width, dash = []) {
        if (Math.hypot(x1 - x0, y1 - y0) < 0.5)
            return;
        const gradient = ctx.createLinearGradient(x0, y0, x1, y1);
        gradient.addColorStop(0, from);
        gradient.addColorStop(1, to);
        ctx.save();
        ctx.strokeStyle = gradient;
        ctx.lineWidth = width;
        ctx.lineCap = 'round';
        ctx.setLineDash(dash);
        ctx.beginPath();
        ctx.moveTo(x0, y0);
        ctx.lineTo(x1, y1);
        ctx.stroke();
        ctx.restore();
    }

    drawAim(ctx, game) {
        if (game.phase !== PHASE.AIMING)
            return;
        // Online, only the player whose turn it is sees the guide. The other
        // player would otherwise see a stale line that cannot be moved.
        if (game.myPlayer >= 0 && game.current !== game.myPlayer)
            return;
        const cue = game.physics.find(0);
        if (!cue || !cue.active || game.winner >= 0)
            return;

        const s = this.scale;
        const r = TABLE.ballRadius * s;
        const cx = this.sx(cue.x);
        const cy = this.sy(cue.y);
        const angle = game.aimAngle;
        const hit = firstHit(game.physics.balls, cue.x, cue.y, angle);

        if (hit) {
            const hx = this.sx(hit.x);
            const hy = this.sy(hit.y);

            // Cue-ball track, from the edge of the ball to the contact point.
            this.guideLine(
                ctx,
                cx + Math.cos(angle) * r,
                cy - Math.sin(angle) * r,
                hx, hy,
                'rgba(255, 255, 255, 0.38)',
                'rgba(255, 214, 10, 0.62)',
                1.3,
                [5, 6],
            );

            if (hit.ball) {
                const guide = contactGuide(hit, angle);
                const bx = this.sx(hit.ball.x);
                const by = this.sy(hit.ball.y);

                // Object-ball line: the short white line every pool game
                // draws to show where the pot goes.
                if (guide) {
                    const ex = this.sx(hit.ball.x + guide.objectDir.x * guide.objectLength);
                    const ey = this.sy(hit.ball.y + guide.objectDir.y * guide.objectLength);
                    this.guideLine(
                        ctx,
                        bx, by, ex, ey,
                        'rgba(255, 255, 255, 0.72)',
                        'rgba(255, 255, 255, 0.08)',
                        2.2,
                        [7, 5],
                    );

                    // Cue ball leaves along the tangent after a cut.
                    if (guide.cueDir && guide.cueLength > 0.02) {
                        const dx = this.sx(hit.x + guide.cueDir.x * guide.cueLength);
                        const dy = this.sy(hit.y + guide.cueDir.y * guide.cueLength);
                        this.guideLine(
                            ctx,
                            hx, hy, dx, dy,
                            'rgba(255, 214, 10, 0.5)',
                            'rgba(255, 214, 10, 0.05)',
                            1.5,
                            [3, 4],
                        );
                    }
                }

                // Ghost ball: where the cue ball has to arrive.
                ctx.save();
                ctx.beginPath();
                ctx.arc(hx, hy, r, 0, Math.PI * 2);
                ctx.fillStyle = 'rgba(255, 255, 255, 0.1)';
                ctx.fill();
                ctx.strokeStyle = 'rgba(255, 214, 10, 0.72)';
                ctx.lineWidth = 1.4;
                ctx.setLineDash([3, 3]);
                ctx.stroke();
                ctx.restore();

                // A small ring around the object ball being struck.
                ctx.beginPath();
                ctx.arc(bx, by, r + 1, 0, Math.PI * 2);
                ctx.strokeStyle = 'rgba(255, 255, 255, 0.3)';
                ctx.lineWidth = 1;
                ctx.stroke();
            } else if (hit.normal) {
                // No ball in the way: show one short cushion bounce.
                const direction = { x: Math.cos(angle), y: Math.sin(angle) };
                const bounced = reflectedDir(direction, hit.normal);
                const length = Math.min(0.45, TABLE.width * 0.18);
                const ex = this.sx(hit.x + bounced.x * length);
                const ey = this.sy(hit.y + bounced.y * length);
                this.guideLine(
                    ctx,
                    hx, hy, ex, ey,
                    'rgba(255, 214, 10, 0.34)',
                    'rgba(255, 214, 10, 0.02)',
                    1.2,
                    [4, 5],
                );
            }
        }

        // Cue stick, pulled back with power.
        const pull = 0.035 + game.aimPower * 0.16;
        const tipDistance = TABLE.ballRadius + 0.012 + pull;
        const tipX = this.sx(cue.x - Math.cos(angle) * tipDistance);
        const tipY = this.sy(cue.y - Math.sin(angle) * tipDistance);
        const length = Math.min(1.5, TABLE.width * 0.55) * s;
        const backX = -Math.cos(angle);
        const backY = Math.sin(angle); // screen y is flipped
        const buttX = tipX + backX * length;
        const buttY = tipY + backY * length;
        const tipW = Math.max(1.6, s * 0.005);
        const buttW = Math.max(3.5, s * 0.014);

        const nx = -backY;
        const ny = backX;

        ctx.save();
        ctx.beginPath();
        ctx.moveTo(tipX + nx * tipW, tipY + ny * tipW);
        ctx.lineTo(buttX + nx * buttW, buttY + ny * buttW);
        ctx.lineTo(buttX - nx * buttW, buttY - ny * buttW);
        ctx.lineTo(tipX - nx * tipW, tipY - ny * tipW);
        ctx.closePath();
        const gradient = ctx.createLinearGradient(tipX, tipY, buttX, buttY);
        gradient.addColorStop(0, '#e8e2d0');
        gradient.addColorStop(0.12, '#c9a06a');
        gradient.addColorStop(0.6, '#8b5a2b');
        gradient.addColorStop(1, '#3c2415');
        ctx.fillStyle = gradient;
        ctx.fill();
        ctx.strokeStyle = 'rgba(0, 0, 0, 0.35)';
        ctx.lineWidth = 0.8;
        ctx.stroke();

        // Tip and ferrule.
        const ferruleLength = Math.min(0.06 * s, length * 0.08);
        ctx.beginPath();
        ctx.moveTo(tipX + nx * tipW, tipY + ny * tipW);
        ctx.lineTo(tipX + backX * ferruleLength + nx * tipW * 1.15,
            tipY + backY * ferruleLength + ny * tipW * 1.15);
        ctx.lineTo(tipX + backX * ferruleLength - nx * tipW * 1.15,
            tipY + backY * ferruleLength - ny * tipW * 1.15);
        ctx.lineTo(tipX - nx * tipW, tipY - ny * tipW);
        ctx.closePath();
        ctx.fillStyle = '#f4f1e6';
        ctx.fill();
        ctx.restore();
    }

    drawCueGhost(ctx, game) {
        if (game.phase !== PHASE.BALL_IN_HAND)
            return;
        const s = this.scale;
        const r = TABLE.ballRadius * s;
        const x = this.sx(game.cueGhost.x);
        const y = this.sy(game.cueGhost.y);
        const legal = !inPocket(game.cueGhost.x, game.cueGhost.y);

        ctx.save();
        ctx.globalAlpha = 0.55;
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.fillStyle = '#ffffff';
        ctx.fill();
        ctx.restore();

        ctx.beginPath();
        ctx.arc(x, y, r + 2, 0, Math.PI * 2);
        ctx.strokeStyle = legal ? this.accent : '#ff5252';
        ctx.lineWidth = 2;
        ctx.setLineDash([3, 4]);
        ctx.stroke();
    }
}

export { BALL_COLORS, RAIL };
