# DOOM sur VTech V.Flash

Portage de LinuxDoom 1.10 pour le processeur ARM926EJ-S de la V.Flash. Le moteur
tourne dans la console émulée par FlashEm. L’archive contient les sources du
port, le moteur ARM précompilé et deux scripts Python.

## Lancer le jeu

Il faut Python 3, l’exécutable autonome **FlashEm** avec l’option `--load` et
un fichier de données **IWAD**. Le port a été testé avec `doom1.wad` de DOOM
shareware 1.9. L’archive contient le moteur et le code de portage. Fournissez
votre propre IWAD.

Depuis le dossier extrait, lancez ces deux commandes en adaptant les chemins.

```sh
python3 tools/pack_wad.py doom-vflash-engine.elf doom1.wad doom-vflash.elf
python3 tools/run_flashem.py /chemin/vers/flashem doom-vflash.elf
```

Sous Windows, utilisez le chemin de `flashem.exe`. Vous pouvez remplacer
`python3` par `py -3` si nécessaire. Si un chemin contient des espaces, mettez-le
entre guillemets.

Le premier script assemble un ELF avec le moteur et l’IWAD, dans la limite
des 16 Mio de RAM de la console. Le second lance l’ELF et fournit à FlashEm un
fichier d’amorçage minimal temporaire. Les journaux du jeu sortent dans le
terminal par l’UART 1 émulé.

## Commandes

| Action | Manette V.Flash | Clavier FlashEm par défaut |
| --- | --- | --- |
| Avancer, reculer et tourner | Direction | Flèches |
| Tirer | Rouge | Z |
| Ouvrir une porte ou confirmer | Vert | C |
| Changer d’arme | Jaune | X |
| Carte ou retour dans les menus | Bleu | V |
| Ouvrir le menu ou confirmer | Enter | Entrée |
| Annuler ou revenir au jeu | Exit | Retour arrière |
| Pas latéral | Gâchettes gauche et droite | Q et W |
| Aide et pause | Question et Livre | H et B |

Vous pouvez sauvegarder et recharger via le menu. Les sauvegardes sont
conservées en RAM pendant la session. Le moteur produit les effets sonores PCM
en stéréo. La musique et le multijoueur restent hors de ce portage. La cible
validée est le chargement ELF dans FlashEm autonome. Le démarrage depuis un
disque ou sur une console physique reste à vérifier.

## Recompiler le moteur

Installez GNU Make, une chaîne ARM GNU `arm-none-eabi` avec newlib et Python 3.
Sous Debian, les paquets utiles sont `gcc-arm-none-eabi`,
`binutils-arm-none-eabi` et `libnewlib-arm-none-eabi`.

```sh
make -j
make game WAD="/chemin/vers/doom1.wad"
```

Le moteur est produit dans `build/doom-vflash-engine.elf`. La seconde commande
génère `build/doom-vflash.elf`. `CROSS_COMPILE` peut désigner un autre préfixe
de chaîne de compilation.

Le moteur est tiré de la publication GPL de LinuxDoom 1.10 par id Software,
au commit `a77dfb96cb91780ca334d0d4cfd86957558007e0`. Voir
[COPYING](COPYING) et le [dépôt d’id Software](https://github.com/id-Software/DOOM).

Le port a été testé avec FlashEm au commit
`cb771f893f6a7e0a9c79adbe1081a0d49be3f44b`, fourni dans l’archive de référence.
Le [dépôt de FlashEm](https://github.com/WizzardSK/flashem-libretro) contient
l’émulateur et ses instructions de compilation.
