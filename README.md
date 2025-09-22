# kind 🧩

**`kind`** est une alternative moderne à la commande Unix `type`.  
Elle affiche des informations détaillées et structurées sur une commande donnée : son type, sa localisation, ses alias éventuels, etc.

> 🔍 Utile pour comprendre rapidement la nature d'une commande, qu'elle soit un alias, une fonction, un binaire ou autre.

---

## ✨ Exemple de sortie

```bash
$ kind ls

Command  : ls
Location : /bin/ls
Type     : binaire
Alias    : alias ls='ls --color=auto'


##  INSTALLATION 

1 .CLONER LE REPOS 
git clone https://github.com/ton-utilisateur/kind.git
cd kind

2 BUILD LE PROJET 
 (AVEC MESON) 

🛠️ Dépendances

 Aucune dépendance externe.
Fonctionne avec un shell POSIX compatible (bash, zsh, etc.).

🧠 Fonctionnalités

✅ Détection du type réel de la commande (alias, function, builtin, binaire)

✅ Affichage lisible et structuré

✅ Compatible avec la plupart des shells

✅ Pratique pour les scripts, le debug ou l'apprentissage du shell 


Comparaison avec type
Commande	Sortie type	Sortie kind
type ls	ls is aliased to 'ls --color=auto'	Affichage structuré avec chemin, etc
type cd	cd is a shell builtin	Plus lisible, détaillé
type foo	-bash: type: foo: not found	Message d'erreur clair

Utilisation 

kind <commande>

exemples : 
kind grep
kind cd
kind ll

imitations

Peut se comporter différemment selon le shell utilisé (bash, zsh, etc.)

Si plusieurs alias sont chaînés, seul le premier niveau peut être affiché (selon l'implémentation)

Le nom kind peut entrer en conflit avec l'outil Kubernetes "kind" sur certaines machines 


TODO: Faire une manpage  (man kind) 
