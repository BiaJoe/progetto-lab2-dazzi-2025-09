#include "utils.h"

// dice se la linea è vuota o no
// non altera il pointer perchè viene passato per valore
// quindi è sicura da usare anche per linee che dopo servono intatte
int is_line_empty(char *line){
	if (line == NULL) return 1;
	while(*line) if(!isspace(*(line++))) return 0;
	return 1;
}

// funzione scitta da me perchè conveniente
// prende una stringa e se essa rappresenta un numero, lo ritorna 
// se non lo rappresenta allora errno = EINVAL;
// gestisce:
// - spazi iniziali
// - segni '-' e '+'
// - valori non accettati (EINVAL)
// non gestisce l'overflow
int my_atoi(char a[]){
	char c;
	int i = 0, sign = 1, res = 0;

	// se la stringa è vuota la rifiuta
	if(a == NULL){
		errno = EINVAL;
		return 0;
	}

	// salta tutti gli spazi vuoti
	for (i = 0; isspace(a[i]); i++) ;		

	// salva il segno del numero
	sign = (a[i] == '-') ? -1 : 1;			
	if(a[i] == '-' || a[i] == '+') 
		i++;

	// controllo che ci sia almeno una cifra
	if(a[i] == '\0'){										
		errno = EINVAL;
		return 0;
	}

	// se trovo una non-cifra scarto tutto
	while ((c = a[i]) != '\0') {
		if (c < '0' || c > '9') {					
			errno = EINVAL;
			return 0;
		}
		res = 10 * res + (c - '0');
		i++;
	}

	// mette il segno e ritona	
	return sign * res;									
}
