# Rules

Rack ships two games. Both are simplified for a fast two-player table: there
are no referees, no called pockets, and no shot clocks. The host's copy of the
game is the only authority, so both players always see the same table.

## 8-Ball

### The rack

- Fifteen object balls in a triangle, with the apex on the foot spot.
- The 8-ball sits in the centre of the third row.
- The two back corners are one solid and one stripe; every other ball is
  shuffled, so the rack is different each game.
- The cue ball starts on the head spot.

### The break

- Player 1 breaks. The table is open until a group is assigned.
- Pocketing a ball on the break does **not** assign a group by itself; the
  first legal pocket after the break decides solids or stripes.
- If the 8-ball is pocketed on the break, the rack is rebuilt and the same
  player breaks again. It is not a loss.

### Turns

- A player keeps shooting while they legally pocket at least one ball from
  their own group.
- Pocketing nothing, or only the opponent's balls, passes the turn.
- Once a group is assigned, the cue ball must contact a ball from that group
  first.

### Fouls

A foul gives the other player **ball in hand anywhere on the table**:

- The cue ball contacts no object ball.
- The cue ball is pocketed (a scratch).
- The 8-ball is contacted first while the table is open.
- A ball from the wrong group is contacted first.
- While on the 8, a ball other than the 8 is contacted first.
- Nothing is pocketed and no ball reaches a cushion after the first contact.

### Winning and losing

- After clearing their group, a player must pocket the 8-ball to win.
- Pocketing the 8-ball early, or on a foul stroke, loses the game for the
  shooter.

## 9-Ball

### The rack

- Balls 1 through 9 in a diamond pointing at the foot spot: rows of
  1, 2, 3, 2, 1.
- The 1-ball is at the apex and the 9-ball in the centre.
- The remaining balls are shuffled.

### Play

- The lowest-numbered ball on the table is always the ball to hit first.
- Any ball pocketed on a legal stroke counts, and the shooter continues.
- Pocketing the 9-ball on a legal stroke wins the game, including on the
  break.
- If no ball is pocketed, the turn passes.

### Fouls

A foul gives the other player **ball in hand anywhere on the table**:

- No object ball is contacted.
- A ball other than the lowest-numbered ball is contacted first.
- The cue ball is pocketed.
- Nothing is pocketed and no ball reaches a cushion after the first contact.

If the 9-ball is pocketed on a foul stroke it is spotted on the foot spot (or
the nearest free space), and the incoming player gets ball in hand.

### Deliberate simplifications

Rack v1 does **not** implement push-out after the break, the three-foul rule,
or call-shot/call-pocket. Those add bookkeeping and slow a casual game down;
everything above is enforced consistently for local, practice, and network
play.

## Practice

Practice uses the same rules with one player. A foul returns the cue ball to
the same player, so you can rehearse a shot without handing the table away.
