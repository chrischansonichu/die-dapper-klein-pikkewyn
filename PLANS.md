# Plans & Ideas

Long-range design notes for Die Dapper Klein Pikkewyn. Not a schedule —
a parking lot for ideas so they don't get lost. Move items into real
issues/branches as they firm up.

## NPC interactivity

More to do with NPCs than one-shot dialogue:

- Repeat conversations that change with story progress (the tutorial's
  staged Ryno machine is the pattern — generalize it beyond the island).
- Small favors/fetch tasks between villagers.
- NPCs that react to what Jan carries or wears (armor, a famous weapon,
  a sangoma charm).
- Schedules/movement so the hub feels alive (fishers out at dawn,
  drying racks tended in the afternoon).

## Weapons

Added gradually after level 1 (the harbor dungeon) — no big dump of
gear. Current arsenal is FishingHook, ShellThrow, SeaUrchinSpike,
Harpoon. Every new weapon slots into the class/damage-type grid
(melee/ranged/magic × blunt/slash/pierce), and gaps in that grid are
the natural place to add:

- A dedicated slashing melee weapon (cutlass shard? propeller blade?)
  — right now ShellThrow is the *only* slash source, so it is also the
  only way to free bound captives and cut kelp. One more slasher gives
  the player a fallback when shells break.
- Higher-tier blunt (an anchor fluke? boat cleat?) for the
  late-dungeon sailor armor.
- Weapon quirks over raw power: the README's "use weapons in unique
  ways" promise — a hook that can also pull items/enemies closer, a
  net that binds instead of damages.

## Magic

Unlocks at the **end of level 2** — no player magic before that.
`ATTACK_CLASS_MAGIC` already exists in the data layer (ColonyRoar,
CrashingTide), so player-facing magic is content + UI, not a new
system. Ideas: water/wave calls, sonic roars, sangoma-taught charms.
Keep it rare and costly — this is a penguin with borrowed magic, not a
wizard.

## Level 2 — the lokasie

- Setting: a lokasie on the shore. Jan has to rescue a penguin being
  kept by a **sangoma**.
- The rescue is the climax of the level; in the course of it the
  rescued penguin **gains some magic** (the sangoma's work rubbing
  off) — this is how magic enters the party, timed with the
  "magic at end of level 2" unlock above.
- The rescued penguin presumably joins as a party member — the first
  magic-class ally (party/recruit flow already exists via the seal).
- Sangoma as an ambiguous figure, not a cartoon villain — worth
  writing carefully.

## Mini games

- **Surfing** — ride a wave to board a ship (late game). Replaces a
  plain map-warp with a skill moment right before a ship assault.
- **??** — open slot. Candidates to consider: a fish-catch/diving game
  (feeds the food-item economy), a shell-skipping or buoy-race game on
  the tutorial island's dare mechanic, a rhythm game for the forge.

## Story

More of it, generally:

- The Annika thread (Jan's origin, the storm, the west-point nest) is
  seeded in the tutorial — it needs payoff later.
- Logbook lore (dungeon 1) works well as environmental storytelling —
  keep planting readable objects.
- Family check-ins: reasons to return to the tutorial island / hub
  between dungeons.

## Puzzles

Beyond combat and fetch: the F6 lantern-dock puzzle is the prototype.
More environmental puzzles per dungeon — light, water levels, tide
timing, pushing/cutting/burning obstacles with the right damage type
(the kelp-needs-slash gate is the pattern: world interactions keyed to
weapon classes/types the player is carrying).
