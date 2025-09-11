# naatal

“Naatal lu fi nekk.”

> ✨ Un clone de la commande `type` pour Linux, fait avec ❤️ pour la communauté sénégalaise.

**naatal** est un outil en ligne de commande qui permet de **révéler** la nature d'une commande shell : est-ce un alias, une fonction, un binaire, un script... ?

Inspiré de la commande Unix `type`, `naatal` s’inscrit dans un esprit de **libre**, de **transparence**, et de **culture locale**.

## 🔧 Fonctionnalités

- Identifie si une commande est :
  - un alias
  - une fonction shell
  - un binaire dans le `$PATH`
- Affiche le chemin complet du binaire
- Format de sortie lisible et localisé

## 🌍 Pourquoi "naatal" ?

> En wolof, _naatal_ signifie **révéler**, **montrer**, **mettre en lumière**.
> C’est exactement ce que fait cet outil : il révèle ce qui se cache derrière une commande.

## 🧪 Exemple

```bash
$ naatal ls
Command  : ls
Location : /bin/ls
Type     : binaire
Alias    : alias  ls='ls --color=auto' 
