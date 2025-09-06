
# Esempi di scenari testabili

---

## Circondati da incendi e terremoti

### Ambiente

queue=emergenze676722
height=100
width=100

### Rescuer types

[Pompieri][30][2][48;48]
[Pegaso][5][15][48;52]
[Ambulanza][30][3][50;50]

### Emergency types

[Incendio Minore] [0] Pompieri:2,4;Ambulanza:1,2;
[Incendio Medio] [1] Pompieri:4,8;Ambulanza:2,5;Pegaso:1,5;
[Incendio Grande] [2] Pompieri:10,10;Ambulanza:5,10;Pegaso:2,5;
[Terremoto Leggero] [0] Ambulanza:3,10;
[Terremoto Intenso] [1] Pompieri:3,5;Ambulanza:3,8;
[Terremoto Distruttivo] [2] Pompieri:10,10;Ambulanza:10,10;Pegaso:3,5;


### File di emergenze

Incendio Minore 84 50 1
Incendio Minore 80 63 1
Incendio Minore 70 73 1
Terremoto Leggero 57 81 1
Terremoto Leggero 43 85 1
Terremoto Leggero 30 81 1
Incendio Medio 18 73 1
Incendio Medio 8 63 1
Incendio Medio 14 50 1
Incendio Grande 18 27 1
Incendio Grande 30 19 1
Incendio Grande 43 15 1
Terremoto Intenso 57 15 1
Terremoto Intenso 70 19 1
Terremoto Intenso 80 27 1
Terremoto Distruttivo 84 36 1
Terremoto Distruttivo 85 50 1
Terremoto Distruttivo 84 64 1

---

## Apocalisse Destra Sinistra

### Ambiente

queue=emergenze676722
height=100
width=200

### Rescuer types

[Pompieri][30][2][10;40]
[Pegaso][5][15][10;45]
[Ambulanza][30][3][10;50]
[Polizia][20][5][10;60]
[Batman][1][10][10;65]
[Superman][1][20][10;70]
[Spiderman][1][2][10;75]
[Esercito][100][3][10;85]

### Emergency types

[Incendio] [2] Pompieri:2,8;
[Sommossa] [2] Pompieri:2,8;Polizia:3,9;
[Infarto] [2] Ambulanza:1,5;
[Incidente] [1] Ambulanza:3,3;Polizia:1,2
[Rapina] [1] Polizia:4,9;
[Terremoto Leggero] [0] Pompieri:1,8;
[Terremoto Intenso] [1] Pompieri:5,8;
[Terremoto Distruttivo] [2] Pompieri:7,8;Pegaso:1,2;
[Joker] [2] Batman:1,10;
[Goblin] [2] Spiderman:1,10;
[Kriptonite] [1] Superman:1,4;
[Godzilla] [2] Superman:1,10;Batman:1,10;Pegaso:1,10;Polizia:5,5;Ambulanza:3,5;Esercito:20,10;
[Guerra] [2] Esercito:30,15;

### File di emergenze

Terremoto Leggero 190 30 1
Terremoto Intenso 190 35 1
Incendio 190 40 1
Sommossa 190 45 1
Rapina 190 50 1
Terremoto Distruttivo 190 60 1
Joker 190 65 1
Goblin 190 67 1
Infarto 190 70 1
Kriptonite 190 75 1
Infarto 190 80 1
Incidente 190 85 1
Guerra 190 90 1
Godzilla 190 55 4