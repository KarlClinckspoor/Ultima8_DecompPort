# Translating Ultima VIII: the rules used for pt-BR

You translate the game's text (dialogue, books, scrolls, item names, messages) from English into
natural, well-written Brazilian Portuguese with a slightly archaic fantasy flavour where the English
has it (Pagan is a grim medieval world). Not a literal word-for-word rendering: make it read as if
written in Portuguese, but keep every fact, name, hint and puzzle clue.

## Format rules (the game breaks if these are wrong)

1. Keep **leading and trailing spaces exactly** as in the English (many strings end with one space;
   they are joined to other strings at run time).
2. Keep the markup characters, in the same order and count where possible:
   `~` = line break (books, scrolls), `*` = line break on gravestones/plaques and pauses in speech,
   `%` = paragraph indent, TAB characters. Never add new ones.
3. Fragments: a string like `My name is ` or `Well met, ` is joined to a name or another string
   by the program. Translate it so the joined result reads well (`Meu nome é `, `Saudações, `).
   Do not add a final period to a fragment that has none.
4. Conversation choices (short strings, usually ending with a space, e.g. `Name `, `Job `,
   `Farewell `, `Tell me of Stellos `) stay short.
5. Allowed characters: plain ASCII plus á à â ã é ê í ó ô õ ú ü ç and Á À Â Ã É Ê Í Ó Ô Õ Ú Ç.
   No other letters, no curly quotes, no — or –, no …, no « ». Use ' and " and - and ... .
6. Spell words of power and incantations (`Kal Des Flam Ylem!`, `In Flam!`, `Vas An Flam Bal Xen!`,
   `Por`, `Ort`, rune names) stay exactly as they are. So do onomatopoeia that work in Portuguese
   (`Hmmm`, `Argh`, `Ugh`).
7. Gravestones and plaques (classes GRAVE_NS, PLAQUENS) are engraved in CAPITALS with no accents
   at all: write `DESCANSE EM PAZ`, `REFEITORIO`, `CORACAO`. Keep each `*`-separated line about as
   short as the English line.
8. Books (class 1427 and others with long texts) must not grow much: aim for no more than about
   10% longer than the English. Prefer concise wording.
9. Keep the same string the same: if two items have identical English, give identical Portuguese.
10. Dialect: the speaker addresses the Avatar as `você` (and `vocês` for many). Elevated beings
   (Titans, the Guardian, daemons) may sound grand, but still use `você`. Archaic English
   (`'tis`, `thee`, `ye`, `ya`) becomes a flavourful but readable Portuguese; rustic speech
   (`tha questions`, dropped letters) can become colloquial Portuguese (`cê`, `tá`, `num`).
11. The Avatar is male.

## Glossary (use these exactly)

Names of people and places stay as they are: Lithos, Stratos, Hydros, Pyros, Mordea, Devon,
Bentic, Salkind, Moriens, Lothian, Vividos, Mythran, Stellos, Cyrrus, Torwin, Gwillim, Malchir,
Corinth, Toran, Vardion, Kilandra, Mesostel, Jenna, Orlok, Aramina, Darion, Kalen, Bane, Beren,
Xavier, Rhian, Daemion, Gorgrond, Khumash-Gor, Yevaud, Keldan, Korick, Corodin, Arcadion,
Tenebrae, Argentrock, Perivolcanae, Aphelion, Carthax, Bloodwatch, Britannia, Pagan, Lleu, Galious.

| English | Português |
|---|---|
| Avatar | Avatar |
| Guardian | Guardião |
| Titan / Titans | Titã / Titãs |
| Titan of Earth / Air / Water / Fire | Titã da Terra / do Ar / da Água / do Fogo |
| Pagan (the world) | Pagan |
| Pagans (its people) | pagãos |
| Zealan / Zealans | zealano / zealanos (adj.: zealano) |
| Tenebraean / Tenebraen | tenebraeano |
| Necromancer / Necromancy / Necromantic | Necromante / Necromancia / necromântico |
| Sorcerer / Sorcery / Sorcerous | Feiticeiro / Feitiçaria / de feitiçaria |
| Theurgist / Theurgy | Teurgo / Teurgia |
| Thaumaturge / Thaumaturgy | Taumaturgo / Taumaturgia |
| Tempest (Mordea's title) / Tempestry | Tempestade / Tempestaria |
| Mountain King | Rei da Montanha |
| the Lurker | o Espreitador |
| Daemon | Demônio |
| Truename | Nome Verdadeiro |
| the Cabal | a Cabala |
| Scion | Herdeiro |
| Acolyte / Disciple / Initiate / Adept / Master | Acólito / Discípulo / Iniciado / Adepto / Mestre |
| Conventicler | Conventículo |
| Enclave | Enclave |
| Destroyer | Destruidor |
| Bone Crusher | Esmaga-Ossos |
| Centeredness (Test of) | Centramento (Teste do) |
| Breath of Wind | Sopro do Vento |
| Heart of Earth / Tongue of Flame / Tear of the Seas | Coração da Terra / Língua de Fogo / Lágrima dos Mares |
| The Calling | O Chamado |
| focus / foci | foco / focos |
| reagent | reagente |
| spellbook | livro de feitiços |
| Blackmoor, Executioner's Hood | Blackmoor, Capuz do Carrasco |
| Pig Iron, Ash, Dragon Blood, Bone, Pumice, Brimstone, Obsidian, Blood, Dirt, Wood | Ferro-Gusa, Cinzas, Sangue de Dragão, Osso, Pedra-Pomes, Enxofre, Obsidiana, Sangue, Terra, Madeira |
| Eye of Newt, Bat Wing, Serpent Scale | Olho de Tritão, Asa de Morcego, Escama de Serpente |
| Blackwine / Black Wine, Tenebraen Ale, Breath o' Spirit | Vinho Negro, Cerveja Tenebraeana, Sopro do Espírito |
| Kith | Kith (a giant spider-like beast) |
| Slayer (sword) | Exterminadora |
| Leggings, Helmet, Arm guards, Shield | Grevas, Elmo, Braçadeiras, Escudo |
| Sabre, Scimitar, Mace, Dagger, Axe, Broadsword | Sabre, Cimitarra, Maça, Adaga, Machado, Espada Larga |

Spells (Sorcery): Flash = Clarão, Ignite = Incendiar, Extinguish = Extinguir, Flame Bolt = Raio de
Fogo, Endure Heat = Suportar Calor, Fire Shield = Escudo de Fogo, Armor of Flames = Armadura de
Chamas, Create Fire = Criar Fogo, Explosion = Explosão, Summon Daemon = Invocar Demônio, Banish
Daemon = Banir Demônio, Conflagration = Conflagração, Invoke Pentagram = Invocar Pentagrama,
Empower ... spell = Fortalecer feitiço ...
Spells (Necromancy): Death Speak = Falar com os Mortos, Mask of Death = Máscara da Morte, Rock
Flesh = Carne de Pedra, Summon Dead = Invocar os Mortos, Open Ground = Abrir o Solo, Create Golem =
Criar Golem, Withstand Death = Resistir à Morte, Grant Peace = Conceder Paz, Call Quake = Invocar
Terremoto.
Spells (Theurgy/Thaumaturgy): Divination = Adivinhação, Healing Touch = Toque Curativo, Aerial
Servant = Servo Aéreo, Reveal = Revelar, Restoration = Restauração, Fade From Sight = Sumir de
Vista, Air Walk = Andar no Ar, Hear Truth = Ouvir a Verdade, Intervention = Intervenção,
Resurrection = Ressurreição, Confusion Blast = Explosão de Confusão, Meteor Shower = Chuva de
Meteoros, Summon Creature = Invocar Criatura, Call Destruction = Invocar Destruição.

Common conversation words: Name = Nome, Job = Ofício, Farewell = Adeus, Greetings = Saudações,
Yes = Sim, No = Não, Nevermind = Esquece.
