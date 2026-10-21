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

## Classes and skill trees

Two party classes, by archetype: **fighter** (Jan, Seal) and **caster**
(Pierie). Those words never show in-game — the player sees "Wave Rider"
and "Tide Caller" (`skill.class.*` in en.lang; names are provisional).
Each class has three trees, three nodes each, bought top to bottom. Every
party member earns **one skill point per finished dungeon**
(`GameStateCompleteDungeon`); a late recruit starts with one point per
dungeon already finished. Spend them in Status → Skills. Table:
`src/data/skill_defs.c`. A node with `SKILL_FX_NONE` is a placeholder — it
cannot be bought and it gates the nodes under it.

Wave Rider trees (✔ = built):

| Riptide (damage) | Breakwater (protect) | Skimmer (ranged) |
| --- | --- | --- |
| ✔ Ambush — sneak attack ×1.5 | ✔ Thick Skin — +3 DEF | ✔ Close Shot — no half damage for ranged at melee distance |
| ✔ Long Reach — melee reaches 2 tiles (needs LOS) | ✔ Swap Places — move: trade tiles with an ally ≤ 3 tiles away (same as ranged weapons) | ✔ Split Shot — ranged hits also deal 50% to the weakest enemy next to the target |
| ✔ Flipper Slap — move: half-power Tackle, 50% chance to stun one turn | ✔ To the Rescue — move: jump next to an ally ≤ 3 tiles away (same as ranged weapons), hit all adjacent enemies, no friendly fire | Through Walls — ranged ignores LOS (undecided) |

Tide Caller trees (Pierie, built 2026-10-21; Tide Mend is innate, so each
tree grants at most one move and the three Special slots suffice):

| Roar (offense) | Mend (support) | Undertow (control) |
| --- | --- | --- |
| ✔ Screech — move: ranged magic, one target, power 55 | ✔ Deep Tide — Tide Mend heals 40% (base 25%) | ✔ Undertow — move: AOE magic over every enemy, power 45 |
| ✔ Loud Voice — magic moves ×1.3 | ✔ Clean Water — Tide Mend cuts ropes, clears daze + snare | ✔ Riptide — Undertow drags each hit enemy one tile closer |
| ✔ Ringing Ears — Screech has the 50% daze | ✔ Second Wind — Tide Mend revives fainted allies at 25% HP | ✔ Swift Current — +2 move tiles per turn |

Magic has no cost today (no MP); "rare and costly" is still open — a
per-battle use cap on magic moves is the likely shape.

Move-granting nodes put their move in a Special slot
(`CombatantSyncSkillMoves`); Special slots went 2 → 3 so all three fit.
Ally moves use `RANGE_ALLY` + `MoveDef.effect`. Stun is `STATUS_STUNNED`.

**Party leader** (`Party.leaderIdx`, read via `PartyLeaderIdx`): picked on
the Status screen. The leader walks the field, makes the sneak attack (so
Ambush now works for Seal too) and shows in the HUD. Jan in slot 0 is still
the protagonist: if Jan falls the fight is lost, whoever leads.

Open:

- Jan and Seal have directional field walk sprites (`player.c`). Any
  later leader (Pierie) needs one too — until then they borrow the battle
  sprite.
- Stun works on bosses at the same 50%. A resist for `canEnrage` creatures
  is parked — add it only if the Captain gets stun-locked.
- The lokasie finale must call `GameStateCompleteDungeon(gs,
  DUNGEON_DONE_LOKASIE)`, map Pierie's creature id to
  `SKILL_CLASS_CALLER` in `SkillClassForCreature`, and append
  `party.leader.tip` to the join dialogue.
- No respec yet. Points are permanent.

## Magic

Unlocks at the **end of level 2** — no player magic before that.
`ATTACK_CLASS_MAGIC` already exists in the data layer (ColonyRoar,
CrashingTide), so player-facing magic is content + UI, not a new
system. Ideas: water/wave calls, sonic roars, sangoma-taught charms.
Keep it rare and costly — this is a penguin with borrowed magic, not a
wizard.

## Level 2 — the lokasie

**Status (2026-10-14): stages 1–6 and the finale landed**
(`src/field/map_lokasie.c`, `maps/lokasie_s1..6.tmx`, tileset
`tools/lokasie.png`; finale staging + resolution in `field.c`, rules in
`battle.c`). The fight plays in the compound yard in front of the
rondavel, not inside it — the S6 tmx already had the yard, and an
interior would have been a seventh map for one scene.

- Setting: **Sinkbaai**, a lokasie on the shore. Six stages: Strandkant
  (beach) → Die Stege (alleys) → Die Sloot (storm-water ditch) → Die
  Werf (scrap yard) → Die Hoofpad (tar road) → Sangoma se Erf (the
  compound). Opens from the hub's east gate once the Captain is beaten;
  the news scene (Pierie missing) plays on the first hub visit after.
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
- **Finale (built 2026-10-14):** knocking on the S6 hut door plays the
  intro, then the yard is staged and the fight starts through the
  captive-rescue path (Pierie is an `NPC_PIERIE` captive, so he joins
  the party the way the seal does). Pierie keeps **Tide Mend** — the
  party's first magic. The sangoma surrenders, repents and turns to
  Jesus Christ, and sits by his door as a friendly resident
  (`LOK_PERSONA_SANGOMA`) on later visits. Ouma's "good man once" line
  is what he returns to.
- Story flags: the first u64 is full (bit 62 = HUB_LEFT). The finale
  opened `GameState.storyFlags2` (`STORY_FLAG2_*`, save v10) — Level 3
  appends there.

### Finale fight design (built 2026-10-14)

The Captain fight is a stat check with one HP-threshold flip (enrage +
summons + forced Cannon Volley). The sangoma fight does not repeat that
shape:

- **Rescue, not kill.** Pierie is on the grid from turn one with
  `STATUS_BOUND` (clears on slash/pierce damage to his cell). The
  sangoma is never killed.
- **Charms gate the fight, not HP.** Three charms (`CREATURE_CHARM`) on
  poles around the yard. They take no turns and only fall to **ranged**
  attacks; a Kettie stone (ranged blunt) drops one in a single hit,
  other thrown weapons chip. If the party has no ranged weapon left at
  all, a flipper works — no softlock on a lost Kettie. While any charm
  stands the sangoma is veiled (a smoke ring is drawn on him) and no
  attack damages him (`ImmuneKey` in battle.c). Win = all three charms
  down → `finaleSurrendered` at round end; nobody has to faint.
- **Charms burn (added 2026-10-21).** At the top of every round each
  standing charm scorches every party member on the eight tiles around
  it (maxHp/8, min 2, halved on easy; bound members are exempt). The
  tiles are tinted orange during the fight so the burn is readable.
- **Crew is the damage, sangoma is the problem.** Yard Boss + brak do
  the hitting. The sangoma works from his doorway and never moves:
  Charm Knot (`MOVE_FX_SNARE`, STATUS_SNARED = lose a turn, 2-turn
  cooldown via `Combatant.aiCooldown`), Draw In (`MOVE_FX_PULL`, drags
  whoever stands next to the bound chick back to his doorway — the
  mirror of the player's Swap Places), Tackle when adjacent.
- **Pierie joins mid-fight.** Cut his ropes and he acts: Tackle + Tide
  Mend (`MOVE_FX_HEAL_PARTY`, 25% max HP to every ally, min 4).
- **Ends with surrender.** One in-battle page when the last charm
  falls, then the surrender speech, Pierie's join, the magic page and
  the skill point as post-battle dialogue.

No party-side summon was needed — the captive-rescue flow covers the
join. Tuning stays low; a child must be able to miss a charm shot and
recover.

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
