#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// définitions des dimensions du jeu
#define taille_carre 20            // taille d'un carré en pixels
#define largeur_jeu 30             // nombre de carrés en largeur
#define hauteur_jeu 20             // nombre de carrés en hauteur
#define largeur_ecran (largeur_jeu * taille_carre)
#define hauteur_ecran (hauteur_jeu * taille_carre)
#define duree_fruit_bonus 50       // durée d'affichage du fruit bonus (5 secondes à 10 fps)

// variables globales pour les médias
Texture2D texture_fond;
Sound son_manger;
Sound son_bonus;
Sound son_game_over;
Sound son_background;
Sound son_vitesse;

// types de fruits que le serpent peut manger
typedef enum {
    fruit_normal = 0,       // fruit normal (pomme)
    fruit_bonus_score,      // bonus: +5 points
    fruit_bonus_vitesse,    // bonus: accélère temporairement le serpent
    fruit_bonus_taille      // bonus: ajoute 3 segments d'un coup
} type_fruit;

// directions possibles pour le serpent
typedef enum {
    dir_haut = 0,
    dir_bas,
    dir_gauche,
    dir_droite
} direction;

// structure pour représenter une position sur la grille
typedef struct {
    int x;  // coordonnée horizontale
    int y;  // coordonnée verticale
} position;

// structure pour représenter une partie du corps du serpent
typedef struct partie_serpent {
    position pos;                    // position du segment
    struct partie_serpent* next;     // pointeur vers le segment suivant
} partie_serpent;

// structure pour représenter un fruit spécial
typedef struct {
    position pos;     // position du fruit
    type_fruit type;  // type du fruit
    int actif;       // est-il actuellement affiché
    int timer;        // temps avant qu'il disparaisse
} fruit;

// structure principale du jeu qui contient tout l'état du jeu
typedef struct {
    // utilisation d'un pointeur générique
    void* premier_segment;        // pointeur vers la tête du serpent
    direction dir_actuelle;       // direction actuelle
    position nourriture;          // position de la nourriture régulière
    fruit fruit_bonus;            // fruit bonus (spécial)
    int score;                    // score du joueur
    int game_over;               //  jeu terminé
    int en_pause;                // jeu en pause
    int en_menu;                 //  dans le menu
    int compteur_fruits;          // compte le temps avant d'afficher un fruit bonus
    int vitesse_bonus_timer;      // durée restante du bonus de vitesse
    int vitesse_normale;          // vitesse normale du jeu en fps
} jeu_snake;

// fonction pour créer un nouveau segment de serpent
partie_serpent* creer_segment(int x, int y) {
    // allouer de la mémoire pour le nouveau segment
    partie_serpent* nouveau = (partie_serpent*)malloc(sizeof(partie_serpent));

    // vérifier si l'allocation a réussi
    if (nouveau == NULL) {
        printf("erreur: impossible d'allouer de la mémoire pour un segment du serpent\n");
        return NULL;  // retourner null en cas d'échec
    }

    // initialiser les valeurs du nouveau segment
    nouveau->pos.x = x;
    nouveau->pos.y = y;
    nouveau->next = NULL;  // pas de segment suivant pour le moment

    return nouveau;  // retourner le pointeur vers le nouveau segment
}

// fonction pour initialiser le jeu
jeu_snake* initialiser_jeu() {
    // allouer de la mémoire pour la structure du jeu
    jeu_snake* jeu = (jeu_snake*)malloc(sizeof(jeu_snake));

    if (jeu ==NULL) {
        printf("erreur: impossible d'allouer de la mémoire pour le jeu\n");
        return NULL;
    }

    // créer la tête du serpent au milieu de l'écran
    partie_serpent* tete = creer_segment(largeur_jeu / 2, hauteur_jeu / 2);
    jeu->premier_segment = tete; // stocker la tête dans le jeu

    // ajouter deux segments supplémentaires au serpent
    // 1. créer le deuxième segment à gauche de la tête
    partie_serpent* deuxieme = creer_segment(tete->pos.x - 1, tete->pos.y);
    // 2. lier la tête au deuxième segment
    tete->next = deuxieme;

    // 3. créer le troisième segment à gauche du deuxième
    partie_serpent* troisieme = creer_segment(deuxieme->pos.x - 1, deuxieme->pos.y);
    // 4. lier le deuxième segment au troisième
    deuxieme->next = troisieme;

    // initialiser les autres paramètres du jeu
    jeu->dir_actuelle = dir_droite;  // le serpent commence vers la droite
    jeu->score = 0;
    jeu->game_over = 0;
    jeu->en_pause = 0;
    jeu->en_menu = 1;  // commencer dans le menu

    // initialiser les paramètres des fruits bonus
    jeu->fruit_bonus.actif = 0;
    jeu->compteur_fruits = 0;
    jeu->vitesse_bonus_timer = 0;
    jeu->vitesse_normale = 10;  // 10 images par seconde

    // placer la première nourriture aléatoirement
    jeu->nourriture.x = GetRandomValue(0, largeur_jeu - 1);
    jeu->nourriture.y = GetRandomValue(0, hauteur_jeu - 1);

    return jeu;
}

// fonction pour libérer la mémoire utilisée par le serpent
void liberer_serpent(partie_serpent* tete) {
    partie_serpent* actuel = tete;     // commencer par la tête
    partie_serpent* suivant;           // pour mémoriser le segment suivant

    // parcourir toute la liste chaînée
    while (actuel != NULL) {
        suivant = actuel->next;        // sauvegarder le pointeur vers le segment suivant
        free(actuel);                  // libérer le segment actuel
        actuel = suivant;              // passer au segment suivant
    }
}

// fonction pour vérifier si une position est occupée par le serpent
int est_sur_serpent(partie_serpent* tete, int x, int y) {
    partie_serpent* actuel = tete;  // commencer par la tête

    // parcourir tous les segments du serpent
    while (actuel != NULL) {
        // vérifier si les coordonnées correspondent
        if (actuel->pos.x == x && actuel->pos.y == y) {
            return 1;  // position occupée par le serpent
        }
        actuel = actuel->next;  // passer au segment suivant
    }

    return 0;  // position libre
}

// fonction pour placer une nouvelle nourriture
void placer_nourriture(jeu_snake* jeu) {
    partie_serpent* tete = (partie_serpent*)jeu->premier_segment;

    // chercher une position libre (pas sur le serpent ni sur le fruit bonus)
    do {
        jeu->nourriture.x = GetRandomValue(0, largeur_jeu - 1);
        jeu->nourriture.y = GetRandomValue(0, hauteur_jeu - 1);
    } while (est_sur_serpent(tete, jeu->nourriture.x, jeu->nourriture.y) ||
             (jeu->fruit_bonus.actif &&
              jeu->fruit_bonus.pos.x == jeu->nourriture.x &&
              jeu->fruit_bonus.pos.y == jeu->nourriture.y));
}

// fonction pour placer un fruit bonus
void placer_fruit_bonus(jeu_snake* jeu) {
    partie_serpent* tete = (partie_serpent*)jeu->premier_segment;

    // choisir aléatoirement un type de fruit bonus
    jeu->fruit_bonus.type = GetRandomValue(fruit_bonus_score, fruit_bonus_taille);

    // chercher une position libre
    do {
        jeu->fruit_bonus.pos.x = GetRandomValue(0, largeur_jeu - 1);
        jeu->fruit_bonus.pos.y = GetRandomValue(0, hauteur_jeu - 1);
    } while (est_sur_serpent(tete, jeu->fruit_bonus.pos.x, jeu->fruit_bonus.pos.y) ||
             (jeu->fruit_bonus.pos.x == jeu->nourriture.x &&
              jeu->fruit_bonus.pos.y == jeu->nourriture.y));

    // activer le fruit et régler sa durée
    jeu->fruit_bonus.actif = 1;
    jeu->fruit_bonus.timer = duree_fruit_bonus;
}

// fonction pour ajouter un segment au serpent à la fin de la liste
void ajouter_segment(jeu_snake* jeu) {
    // récupérer la tête
    partie_serpent* tete = (partie_serpent*)jeu->premier_segment;
    partie_serpent* actuel = tete;
    // trouver le dernier segment
    while (actuel->next != NULL) {
        actuel = actuel->next;
    }
    // créer un nouveau segment
    partie_serpent* nouveau = creer_segment(actuel->pos.x, actuel->pos.y);
    // lier le dernier segment au nouveau
    actuel->next = nouveau;
}

// fonction appelée quand le serpent mange un fruit bonus
void consommer_fruit_bonus(jeu_snake* jeu) {
    PlaySound(son_bonus);  // jouer le son du bonus

    // différents effets selon le type de fruit
    switch (jeu->fruit_bonus.type) {
        case fruit_bonus_score:
            // ajoute 5 points supplémentaires
            jeu->score += 5;
            break;

        case fruit_bonus_vitesse:
            // accélère temporairement le serpent
            jeu->vitesse_bonus_timer = 50;  // 5 secondes à 10 fps
            SetTargetFPS(15);  // augmente la vitesse à 15 fps
            PlaySound(son_vitesse);
            break;

        case fruit_bonus_taille:
            // ajoute 3 segments d'un coup
            for (int i = 0; i < 3; i++) {
                ajouter_segment(jeu);
            }
            break;

        default:
            break;
    }

    // désactiver le fruit bonus après consommation
    jeu->fruit_bonus.actif = 0;
}

// fonction pour gérer l'état des fruits bonus
void mise_a_jour_fruits_bonus(jeu_snake* jeu) {
    // si un fruit bonus est actif, diminuer son timer
    if (jeu->fruit_bonus.actif) {
        jeu->fruit_bonus.timer--;

        // si le timer atteint zéro, désactiver le fruit
        if (jeu->fruit_bonus.timer <= 0) {
            jeu->fruit_bonus.actif = 0;
        }
    }
    // si aucun fruit bonus n'est actif, gérer l'apparition d'un nouveau
    else {
        jeu->compteur_fruits++;

        // faire apparaître un fruit bonus après un certain temps aléatoire
        if (jeu->compteur_fruits >= GetRandomValue(80, 120)) {
            placer_fruit_bonus(jeu);
            jeu->compteur_fruits = 0;  // réinitialiser le compteur
        }
    }

    // gérer le bonus de vitesse
    if (jeu->vitesse_bonus_timer > 0) {
        jeu->vitesse_bonus_timer--;

        // si le timer atteint zéro, revenir à la vitesse normale
        if (jeu->vitesse_bonus_timer <= 0) {
            SetTargetFPS(jeu->vitesse_normale);
        }
    }
}

// fonction principale pour déplacer le serpent
void deplacer_serpent(jeu_snake* jeu) {
    // récupérer la tête du serpent
    partie_serpent* tete = (partie_serpent*)jeu->premier_segment;

    // ne rien faire si le jeu est en pause ou terminé
    if (jeu->en_pause || jeu->game_over) return;

    // étape 1: mémoriser la position actuelle de la tête
    int ancien_x = tete->pos.x;
    int ancien_y = tete->pos.y;

    // étape 2: déplacer la tête dans la direction choisie
    switch (jeu->dir_actuelle) {
        case dir_haut:    tete->pos.y--; break;
        case dir_bas:     tete->pos.y++; break;
        case dir_gauche:  tete->pos.x--; break;
        case dir_droite:  tete->pos.x++; break;
    }

    // étape 3: vérifier collision avec les murs
    if (tete->pos.x < 0 || tete->pos.x >= largeur_jeu ||
        tete->pos.y < 0 || tete->pos.y >= hauteur_jeu) {
        jeu->game_over = 1;
        PlaySound(son_game_over);
        return;  // sortir de la fonction si game over
    }

    // étape 4: vérifier collision avec le serpent lui-même
    partie_serpent* actuel = tete->next;  // commencer par le segment après la tête
    while (actuel !=NULL) {
        if (tete->pos.x == actuel->pos.x && tete->pos.y == actuel->pos.y) {
            jeu->game_over = 1;
            PlaySound(son_game_over);
            return;  // sortir de la fonction si game over
        }
        actuel = actuel->next;
    }

    // étape 5: déplacer le corps du serpent
    /*suivre chaque segment en lui donnant la position
    qu'avait le segment précédent avant le déplacement*/
    actuel = tete->next;  // commencer par le segment après la tête
    int temp_x, temp_y;

    while (actuel !=NULL) {
        // sauvegarder la position actuelle de ce segment
        temp_x = actuel->pos.x;
        temp_y = actuel->pos.y;

        // déplacer ce segment à la position du segment précédent
        actuel->pos.x = ancien_x;
        actuel->pos.y = ancien_y;

        // préparer pour le segment suivant
        ancien_x = temp_x;
        ancien_y = temp_y;

        // passer au segment suivant
        actuel = actuel->next;
    }

    // étape 6: vérifier si la nourriture normale a été mangée
    if (tete->pos.x == jeu->nourriture.x && tete->pos.y == jeu->nourriture.y) {
        ajouter_segment(jeu);    // grandir le serpent
        placer_nourriture(jeu);  // placer une nouvelle nourriture
        jeu->score++;            // augmenter le score
        PlaySound(son_manger);   // jouer le son
    }

    // étape 7: vérifier si un fruit bonus a été mangé
    if (jeu->fruit_bonus.actif &&
        tete->pos.x == jeu->fruit_bonus.pos.x &&
        tete->pos.y == jeu->fruit_bonus.pos.y) {
        consommer_fruit_bonus(jeu);
    }

    // étape 8: mettre à jour l'état des fruits bonus
    mise_a_jour_fruits_bonus(jeu);
}

// fonction pour dessiner le jeu
void dessiner_jeu(jeu_snake* jeu) {
    // récupérer la tête du serpent
    partie_serpent* tete = (partie_serpent*)jeu->premier_segment;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // afficher l'image de fond
    DrawTexture(texture_fond, 0, 0, WHITE);

    // si nous sommes dans le menu
    if (jeu->en_menu) {
        DrawText("jeu du serpent", largeur_ecran/2 - MeasureText("jeu du serpent", 40)/2, hauteur_ecran/4, 40, BLACK);
        DrawText("appuyez sur entree pour jouer", largeur_ecran/2 - MeasureText("appuyez sur entree pour jouer", 20)/2, hauteur_ecran/2, 20, BLACK);
        DrawText("utilisez les fleches pour diriger le serpent", largeur_ecran/2 - MeasureText("utilisez les fleches pour diriger le serpent", 20)/2, hauteur_ecran/2 + 30, 20, BLACK);
        DrawText("p pour mettre en pause", largeur_ecran/2 - MeasureText("p pour mettre en pause", 20)/2, hauteur_ecran/2 + 60, 20, BLACK);
        DrawText("attention: le serpent meurt s'il touche un mur", largeur_ecran/2 - MeasureText("attention: le serpent meurt s'il touche un mur", 20)/2, hauteur_ecran/2 + 90, 20, RED);
        EndDrawing();
        return;
    }

    // dessiner la grille de jeu
    for (int i = 0; i < largeur_jeu; i++) {
        for (int j = 0; j < hauteur_jeu; j++) {
            DrawRectangleLines(i * taille_carre, j * taille_carre, taille_carre, taille_carre, LIGHTGRAY);
        }
    }

    // dessiner la nourriture normale
    DrawRectangle(jeu->nourriture.x * taille_carre, jeu->nourriture.y * taille_carre, taille_carre, taille_carre, RED);

    // dessiner le fruit bonus s'il est actif
    if (jeu->fruit_bonus.actif) {
       Color couleur_fruit;

        // choisir la couleur selon le type de fruit
        switch (jeu->fruit_bonus.type) {
            case fruit_bonus_score:   couleur_fruit = GOLD;   break;
            case fruit_bonus_vitesse: couleur_fruit = BLUE;   break;
            case fruit_bonus_taille:  couleur_fruit = PURPLE; break;
            default:                  couleur_fruit = ORANGE;
        }

        // faire clignoter le fruit quand il va disparaître
        if (jeu->fruit_bonus.timer < 20 && (jeu->fruit_bonus.timer / 3) % 2 == 0) {
            couleur_fruit.a = 128; // semi-transparent
        }

        DrawRectangle(jeu->fruit_bonus.pos.x * taille_carre, jeu->fruit_bonus.pos.y * taille_carre,
                     taille_carre, taille_carre, couleur_fruit);
    }

    // dessiner le serpent - parcourir la liste chaînée
    partie_serpent* actuel = tete;
    while (actuel != NULL) {
        // dessiner la tête en vert foncé, le corps en vert
        Color couleur;
        if (actuel == tete) {
            couleur = DARKGREEN;
          }
        else {
            couleur = GREEN;
            }
        DrawRectangle(actuel->pos.x * taille_carre, actuel->pos.y * taille_carre,
                     taille_carre, taille_carre, couleur);
        actuel = actuel->next;  // passer au segment suivant
    }

    // afficher le score
    char texte_score[20];
    //  afficher le score
    printf("score: %d\n", jeu->score);
    // mais pour l'affichage dans la fenêtre, on doit toujours préparer le texte
    texte_score[0] = 's';
    texte_score[1] = 'c';
    texte_score[2] = 'o';
    texte_score[3] = 'r';
    texte_score[4] = 'e';
    texte_score[5] = ':';
    texte_score[6] = ' ';

    // convertir le score en texte
    int temp_score = jeu->score;
    int position = 7;

    if (temp_score == 0) {
        texte_score[position++] = '0';
        texte_score[position] = '\0';
    } else {
        char chiffres[10];
        int nb_chiffres = 0;

        while (temp_score > 0) {
            chiffres[nb_chiffres++] = '0' + (temp_score % 10);
            temp_score /= 10;
        }

        // copier les chiffres dans l'ordre inverse
        for (int i = nb_chiffres - 1; i >= 0; i--) {
            texte_score[position++] = chiffres[i];
        }
        texte_score[position] = '\0';
    }

    DrawText(texte_score, 10, 10, 20, BLACK);

    // afficher l'indicateur de bonus de vitesse
    if (jeu->vitesse_bonus_timer > 0) {
        DrawText("vitesse bonus", largeur_ecran - MeasureText("vitesse bonus", 20) - 10, 10, 20,BLUE);
    }

    // afficher le temps restant pour le fruit bonus
    if (jeu->fruit_bonus.actif) {
        char texte_timer[20];
        // préparer le texte du timer
        texte_timer[0] = 'f';
        texte_timer[1] = 'r';
        texte_timer[2] = 'u';
        texte_timer[3] = 'i';
        texte_timer[4] = 't';
        texte_timer[5] = ':';
        texte_timer[6] = ' ';

        // convertir le timer en texte
        int temps_restant = jeu->fruit_bonus.timer/10 + 1;
        int position = 7;

        if (temps_restant == 0) {
            texte_timer[position++] = '0';
            texte_timer[position] = '\0';
        } else {
            // stocker les chiffres dans un buffer temporaire
            char chiffres[10];
            int nb_chiffres = 0;

            while (temps_restant > 0) {
                chiffres[nb_chiffres++] = '0' + (temps_restant % 10);
                temps_restant /= 10;
            }

            // copier les chiffres dans l'ordre inverse
            for (int i = nb_chiffres - 1; i >= 0; i--) {
                texte_timer[position++] = chiffres[i];
            }
            texte_timer[position] = '\0';
        }

        DrawText(texte_timer, 10, 40, 20, PURPLE);
    }

    // afficher le message de pause
    if (jeu->en_pause) {
        DrawText("pause", largeur_ecran/2 - MeasureText("pause", 40)/2, hauteur_ecran/2 - 40, 40, BLACK);
        DrawText("appuyez sur p pour continuer", largeur_ecran/2 - MeasureText("appuyez sur p pour continuer", 20)/2, hauteur_ecran/2 + 20, 20,BLACK);
    }

    // afficher le message de game over
    if (jeu->game_over) {
        DrawText("game over", largeur_ecran/2 - MeasureText("game over", 40)/2, hauteur_ecran/2 - 40, 40, BLACK);
        DrawText("appuyez sur entree pour rejouer", largeur_ecran/2 - MeasureText("appuyez sur entree pour rejouer", 20)/2, hauteur_ecran/2 + 20, 20, BLACK);
    }

    EndDrawing();
}

// fonction pour charger les sons
void charger_sons() {
    // initialiser le système audio
    InitAudioDevice();

    // charger les sons
    son_manger = LoadSound("C:/Users/pc/OneDrive/Bureau/PROJET S2/final/bin/Debug/sons/manger.wav");
    son_bonus = LoadSound("C:/Users/pc/OneDrive/Bureau/PROJET S2/final/bin/Debug/sons/bonus.wav");
    son_game_over = LoadSound("C:/Users/pc/OneDrive/Bureau/PROJET S2/final/bin/Debug/sons/game_over.wav");
    son_vitesse = LoadSound("C:/Users/pc/OneDrive/Bureau/PROJET S2/final/bin/Debug/sons/vitesse.wav");
    son_background = LoadSound("C:/Users/pc/OneDrive/Bureau/PROJET S2/final/bin/Debug/sons/background.wav");
}

// fonction pour décharger les sons
void decharger_sons() {
    // libérer la mémoire des sons
    UnloadSound(son_manger);
    UnloadSound(son_bonus);
    UnloadSound(son_game_over);
    UnloadSound(son_vitesse);
    UnloadSound(son_background);

    // fermer le système audio
    CloseAudioDevice();
}

// fonction principale
int main() {
    // initialiser la fenêtre
    InitWindow(largeur_ecran, hauteur_ecran, "jeu du serpent");

    // charger l'image de fond
    texture_fond = LoadTexture("C:/Users/pc/OneDrive/Bureau/PROJET S2/final/bin/Debug/background.png");
    // charger les sons
    charger_sons();
    // configurer le jeu
    SetTargetFPS(10);  // 10 images par seconde
    srand(time(NULL)); // initialiser le générateur de nombres aléatoires

    // créer et initialiser le jeu
    jeu_snake* jeu = initialiser_jeu();

    // boucle principale du jeu
    while (!WindowShouldClose()) {
        // gestion du menu principal
        if (jeu->en_menu) {
            if (IsKeyPressed(KEY_ENTER)) {
                jeu->en_menu = 0;
            PlaySound(son_background);  // démarrer la musique
            }
        }
        // gestion de l'écran de fin de jeu
        else if (jeu->game_over) {
            if (IsKeyPressed(KEY_ENTER)) {
                // recommencer une partie
                liberer_serpent((partie_serpent*)jeu->premier_segment);  // libérer la mémoire du serpent actuel
                free(jeu);                   // libérer la structure du jeu
                jeu = initialiser_jeu();     // créer un nouveau jeu
                jeu->en_menu = 0;        // ne pas retourner au menu
                SetTargetFPS(10);            // réinitialiser la vitesse
                PlaySound(son_background);   // redémarrer la musique
            }
        }
        // jeu en cours
        else {
            // gestion de la pause
            if (IsKeyPressed(KEY_P)) {
                jeu->en_pause = !jeu->en_pause;  // inverser l'état de pause
            }

            // gestion des contrôles de direction
            // on empêche de faire demi-tour directement
            if (IsKeyPressed(KEY_UP) && jeu->dir_actuelle != dir_bas) {
                jeu->dir_actuelle = dir_haut;
            }
            if (IsKeyPressed(KEY_DOWN) && jeu->dir_actuelle != dir_haut) {
                jeu->dir_actuelle = dir_bas;
            }
            if (IsKeyPressed(KEY_LEFT) && jeu->dir_actuelle != dir_droite) {
                jeu->dir_actuelle = dir_gauche;
            }
            if (IsKeyPressed(KEY_RIGHT) && jeu->dir_actuelle != dir_gauche) {
                jeu->dir_actuelle = dir_droite;
            }

            // déplacer le serpent si le jeu n'est pas en pause
            if (!jeu->en_pause) {
                deplacer_serpent(jeu);
            }
        }

        // afficher le jeu
        dessiner_jeu(jeu);
    }

    // nettoyage à la fin du jeu
    liberer_serpent((partie_serpent*)jeu->premier_segment);  // libérer la mémoire du serpent
    free(jeu);                   // libérer la structure du jeu
    UnloadTexture(texture_fond); // décharger la texture
    decharger_sons();            // décharger les sons
    CloseWindow();               // fermer la fenêtre

    return 0;
}
