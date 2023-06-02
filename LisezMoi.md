# Rapport du projet MiniShell

## Sommaire

1. [Architecture de l'application](#architecture-de-lapplication)
2. [Choix de conception et spécificités](#choix-de-conception-et-specificités)
3. [Problèmes rencontrés](#problemes-rencontres)
4. [Tests](#tests)

## Architecture de l'application

<Expliquer ici l'organisation générale de votre code, les principaux modules et comment ils interagissent>

## Choix de conception et spécificités

<Discuter ici des décisions de conception que vous avez prises et pourquoi. Parlez également des aspects spécifiques ou uniques de votre implémentation>

## Problèmes rencontrés

<Expliquer ici les problèmes que vous avez rencontrés lors du développement du projet et comment vous les avez résolus>

## Tests

### Tester une commande simple

```
ls
```

### Tester une commande avec des arguments

```
ls -l
```

### Tester une commande qui change l'état du shell

```
cd /
```

### Tester le comportement de la commande `exit`

```
exit
```

### Tester une commande en tâche de fond

```
sleep 10 &
```

### Tester les redirections en écrivant le contenu d'un répertoire dans un fichier

```
ls > file.txt
```

### Tester les redirections en lisant le contenu d'un fichier

```
cat < file.txt
```

### Tester les tubes simples

```
ls | wc -l
```

### Tester les pipelines

```
cat file.txt | grep "test" | wc -l
```

### Tester les commandes internes `lj``sj``bg``fg`

```
lj
sleep 30 &
lj
sj [id]
lj
bg [id]
lj
fg [id]
```