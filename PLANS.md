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
  fish pens tended at low tide).

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

**Status (2026-09-09): stages 1–6 landed** (`src/field/map_lokasie.c`,
`maps/lokasie_s1..6.tmx`, tileset `tools/lokasie.png`). The finale is
still open — see "Next" below.

- Setting: **Sinkbaai**, a lokasie on the shore. Six stages: Strandkant
  (beach) → Die Stege (alleys) → Die Sloot (storm-water ditch) → Die
  Werf (scrap yard) → Die Hoofpad (tar road) → Sangoma se Erf (the
  compound). Opens from the hub's east gate once the Captain is beaten;
  the news scene (Lappies missing) plays on the first hub visit after.
- Opponents are the **sangoma's crew**, not the residents: Skollie
  (Kettie), Lookout (Kettie, stands watch), Yard Boss (Knobkierie), Brak
  (yard dog), plus the perlemoen poachers from Level 1 in the surf.
  Residents (Thandi, Ouma Nomsa, Bra Vusi) are friendly NPCs who hand
  out the hints. Keep it that way — the elder says it in-game: "not
  everyone in Sinkbaai is your enemy."
- Hidden items behind **weapon-kind gates** (the kelp-slash rule
  generalized): a red paraffin drum needs anything thrown from range
  (blows the wall behind it), a wired fence gap needs slash, a rusted
  padlock needs a blunt *weapon*. Kettie / snoek / perlemoen / spare
  kettie in the four pockets.
- New weapons: **Knobkierie** (melee blunt, tier 2 — first real blunt
  weapon; drops 100% from the S4 yard boss) and **Kettie** (ranged blunt,
  tier 1). Both fill the last empty cells of the class×type grid and are
  meant to keep mattering: blunt for locks/crates, ranged blunt for
  knocking things down (bells, lanterns, hanging stuff) in later levels.
- **Next:** the finale in the rondavel. Jan has to rescue **Lappies**
  from the **sangoma**. In the course of it the rescued penguin **gains
  some magic** (the sangoma's work rubbing off) — this is how magic
  enters the party, timed with the "magic at end of level 2" unlock
  above. Lappies presumably joins as the first magic-class ally
  (party/recruit flow already exists via the seal). Sangoma as an
  ambiguous figure, not a cartoon villain — worth writing carefully;
  Ouma's line ("a good man once, before the yard boys started paying
  him") is the seed. The S6 hut door is a placeholder object
  (`OBJ_HUT_DOOR`, `lok.hut.door.*`) until then.
- Story flags: bits 45–61 are spoken for; **bits 62–63 are the last two
  in the u64** — the finale or Level 3 needs a second flag word in
  GameState + a save bump.

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
