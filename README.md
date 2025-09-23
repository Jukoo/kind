# kind 
_Une alternative moderne à la commande Unix `type`_

**kind** est une commande shell moderne qui permet d’inspecter en détail la nature d’une commande Unix. Elle fournit des informations structurées et lisibles sur :
* Le type de la commande (alias, mot-cle du shell, builtin, binaire, script)
* Sa localisation
* Les alias associés
* etc ... 

**Utile pour comprendre rapidement ce qu’est réellement une commande**, que ce soit pour le débogage, l’apprentissage ou l’écriture de scripts.
## Exemple de sortie 
```bash 
$ kind ls
Command  : ls  
Location : /bin/ls  
Type     : binaire  
Alias    : alias ls='ls --color=auto'
```

## ⚙️ Installation
1. Cloner le dépôt
   ```bash
   git clone https://github.com/Jukoo/kind.git
   cd kind
   ```
2. Compiler le project (avec [Meson](https://mesonbuild.com/)) 
   ```bash
   meson  setup build
   meson compile -C build
   ```
3. Vous pouvez installer ou le desinstaller  le projet en local  
```bash 
  meson install -C build    # pour installer 
  ninja -C build uninstall  # pour desinstaller 
```

## Dépendances
Aucune dépendance externe.

## Fonctionnalités

- [x] Détection précise du type de commande : alias, mot-cle du shell,builtin,binaire ,script
- [x] Affichage structuré et lisible
- [x] Compatible avec la plupart des shells 
- [x] Idéal pour : le debug, les scripts, ou l’apprentissage du shell

### Comparaison avec `type` 

| Commande   | Sortie de `type`                     | Sortie de `kind`                             |
| ---------- | ------------------------------------ | -------------------------------------------- |
| `type ls`  | `ls is aliased to 'ls --color=auto'` | Affichage structuré avec chemin, alias, etc. |
| `type cd`  | `cd is a shell builtin`              | Plus lisible et détaillé                     |
| `type foo` | `-bash: type: foo: not found`        | Message d’erreur clair et explicite          |


`kind` va plus loin que la simple commande `type` en offrant un affichage structuré et des indices utiles.

| Commande   | Sortie de `type`             
|------------|-----------------------------|
| `type ldd` | `ldd is /usr/bin/ldd`       | 


| Commande   | Sortie de `kind`                                        |
|------------|----------------------------------------------------------
| `kind ldd` |  Command:  ldd                                          |
|            |  Location : /usr/bin <is potentially a script>          |
|            |  Type : [:script:]                                      |
|            |  Hint : Please use 'file' command to investigate further|
                                          

#### Ce que ça montre

- **`type`** se contente d’afficher l’emplacement ou le statut général de la commande.
- **`kind`** va plus loin :
  - détecte et précise qu’il s’agit potentiellement d’un script (pas seulement d’un binaire),
  - affiche un format structuré et lisible (command, location, type),
  - fournit des **indices contextuels** (“Hint”) pour aller plus loin (ici suggérer `file` pour inspecter le contenu du script).

En résumé, **kind** ne se limite pas à dire *où* se trouve la commande, il aide à comprendre *ce qu’elle est vraiment*.


## Utilisation 

```bash
kind <commande> 
```

## Limitations
> [!WARNING]
> _Étant donné que l'environnement de développement de cette commande a été entièrement créé à l'aide de bash_,
> le comportement peut varier légèrement selon le shell utilisé (**zsh**, **fish**, etc...),
> si plusieurs alias sont chaînés, seul le premier niveau peut être affiché.

> [!IMPORTANT]
> Le nom *kind* peut entrer en *conflit avec l’outil Kubernetes* [kind](https://kind.sigs.k8s.io) sur certaines machines
>

*Suggestions ou des Améliorations via des issues ou pull requests sur le dépôt.
   
