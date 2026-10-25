# Game profiles

A profile describes one game's counters. The firmware is a generic counter
engine, and profiles are data so new games don't need a firmware update.

Draft format (JSON for editing; may be packed as CBOR on-device):

```json
{
  "id": "riftbound",
  "name": "Riftbound",
  "version": 1,
  "players": 2,
  "counters": [
    { "id": "points", "label": "Points", "start": 0, "min": 0, "win_at": 8, "primary": true,
      "kinds": ["Conquer", "Hold"] }
  ]
}
```

```json
{
  "id": "mtg-commander",
  "name": "Magic: Commander",
  "version": 1,
  "players": 4,
  "counters": [
    { "id": "life", "label": "Life", "start": 40, "lose_at": 0, "primary": true },
    { "id": "poison", "label": "Poison", "start": 0, "min": 0, "lose_at": 10 }
  ]
}
```

Starter set to define: Riftbound, Magic (20 / Commander 40), Lorcana (lore to
20), Flesh and Blood, Star Wars: Unlimited.

`kinds` (optional) names the ways a counter can change. Each change is tagged
with one, and the device keeps a tally per kind (Riftbound: points from
Conquer vs Hold).

`group` (default true) merges rapid taps into one history entry (five taps of
−1 become −5). Riftbound sets it to false so every point is recorded on its
own, in order.

Every counter change is appended to a per-game history log so the device
supports undo and can show how a total was reached.
