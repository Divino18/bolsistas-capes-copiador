/*
 * copiador.c
 *
 * Programa para copiar registros de bolsistas de um arquivo CSV de origem
 * para um arquivo CSV de destino, evitando duplicidade de nomes.
 *
 * A verificacao de redundancia e feita por BUSCA SEQUENCIAL: para cada
 * nome lido do arquivo de origem, comparamos (usando strcmp) contra cada
 * nome ja presente no arquivo de destino (incluindo a linha de cabecalho,
 * que tambem e varrida durante a busca). O numero total de comparacoes
 * realizadas e contabilizado e registrado, ao final, no arquivo de
 * estatisticas.
 *
 * Uso:
 *   ./copiador <origem.csv> <destino.csv> <estatisticas.csv>
 *
 * Formato de cada linha do CSV:
 *   Nome,Modalidade,Nivel,Agencia
 *
 * Compilacao:
 *   gcc -Wall -Wextra -O2 -o copiador copiador.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 2048

/* ---------------------------------------------------------------------
 * Estrutura de dados: lista dinamica de nomes ja conhecidos no destino.
 * O indice 0 sempre corresponde ao campo "Nome" do cabecalho do CSV,
 * pois a implementacao considera o cabecalho como a primeira linha
 * varrida durante a busca sequencial.
 * --------------------------------------------------------------------- */
typedef struct {
    char **names;
    int count;
    int capacity;
} NameList;

/* Remove '\n' e '\r' do final da string, in-place. */
static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/*
 * Extrai o primeiro campo (Nome) de uma linha CSV, delimitado pela
 * primeira virgula encontrada. Retorna uma string alocada dinamicamente
 * que deve ser liberada pelo chamador com free().
 */
static char *extract_first_field(const char *line) {
    const char *comma = strchr(line, ',');
    size_t len = comma ? (size_t)(comma - line) : strlen(line);

    char *field = (char *) malloc(len + 1);
    if (!field) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(field, line, len);
    field[len] = '\0';
    return field;
}

/* Inicializa a NameList com capacidade inicial. */
static void namelist_init(NameList *nl) {
    nl->capacity = 256;
    nl->count = 0;
    nl->names = (char **) malloc(sizeof(char *) * (size_t) nl->capacity);
    if (!nl->names) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
}

/* Adiciona um nome (copia propria) ao final da NameList, com realocacao
 * automatica quando necessario. */
static void namelist_add(NameList *nl, const char *name) {
    if (nl->count >= nl->capacity) {
        nl->capacity *= 2;
        char **tmp = (char **) realloc(nl->names, sizeof(char *) * (size_t) nl->capacity);
        if (!tmp) {
            fprintf(stderr, "ERRO: falha de realocacao de memoria.\n");
            exit(EXIT_FAILURE);
        }
        nl->names = tmp;
    }

    size_t len = strlen(name);
    char *copy = (char *) malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "ERRO: falha de alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, name, len + 1);

    nl->names[nl->count] = copy;
    nl->count++;
}

/* Libera toda a memoria associada a NameList. */
static void namelist_free(NameList *nl) {
    for (int i = 0; i < nl->count; i++) {
        free(nl->names[i]);
    }
    free(nl->names);
    nl->names = NULL;
    nl->count = 0;
    nl->capacity = 0;
}

/*
 * Busca sequencial de "name" dentro da NameList "nl".
 * Cada comparacao realizada (strcmp) incrementa o contador global
 * apontado por "comparisons". Retorna 1 se encontrado, 0 caso contrario.
 */
static int sequential_search(const NameList *nl, const char *name, long *comparisons) {
    for (int i = 0; i < nl->count; i++) {
        (*comparisons)++;
        if (strcmp(nl->names[i], name) == 0) {
            return 1; /* encontrado: nome redundante */
        }
    }
    return 0; /* nao encontrado */
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <origem.csv> <destino.csv> <estatisticas.csv>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_path   = argv[1];
    const char *dst_path   = argv[2];
    const char *stats_path = argv[3];

    /* ---------------- Abertura do arquivo de origem ---------------- */
    FILE *src = fopen(src_path, "r");
    if (!src) {
        fprintf(stderr, "ERRO: nao foi possivel abrir o arquivo de origem '%s'\n", src_path);
        return EXIT_FAILURE;
    }

    /* ---------------- Carregamento do arquivo de destino ----------- */
    /* Se o destino ja existir, seus nomes (inclusive o cabecalho) sao
     * carregados em memoria para permitir busca sequencial eficiente,
     * sem reabrir o arquivo em disco a cada comparacao. */
    NameList existing;
    namelist_init(&existing);

    char line[MAX_LINE];
    char header_line[MAX_LINE];
    int dest_exists = 0;

    FILE *dst_read = fopen(dst_path, "r");
    if (dst_read) {
        dest_exists = 1;

        if (fgets(line, sizeof(line), dst_read)) {
            strip_newline(line);
            strncpy(header_line, line, sizeof(header_line) - 1);
            header_line[sizeof(header_line) - 1] = '\0';

            char *name = extract_first_field(line);
            namelist_add(&existing, name); /* cabecalho: indice 0 */
            free(name);
        }

        while (fgets(line, sizeof(line), dst_read)) {
            strip_newline(line);
            if (strlen(line) == 0) continue;

            char *name = extract_first_field(line);
            namelist_add(&existing, name);
            free(name);
        }
        fclose(dst_read);
    }

    if (!dest_exists) {
        strcpy(header_line, "Nome,Modalidade,Nivel,Agencia");
        namelist_add(&existing, "Nome");
    }

    /* Abre o destino para escrita: em modo "append" se ja existia
     * (preserva conteudo previamente carregado), ou "w" para criar um
     * novo arquivo com cabecalho. */
    FILE *dst = fopen(dst_path, dest_exists ? "a" : "w");
    if (!dst) {
        fprintf(stderr, "ERRO: nao foi possivel abrir o arquivo de destino '%s'\n", dst_path);
        fclose(src);
        namelist_free(&existing);
        return EXIT_FAILURE;
    }
    if (!dest_exists) {
        fprintf(dst, "%s\n", header_line);
    }

    /* ---------------- Leitura e descarte do cabecalho da origem ---- */
    if (!fgets(line, sizeof(line), src)) {
        fprintf(stderr, "ERRO: arquivo de origem vazio ou invalido.\n");
        fclose(src);
        fclose(dst);
        namelist_free(&existing);
        return EXIT_FAILURE;
    }

    /* ---------------- Processamento principal ----------------------- */
    long total_comparisons = 0;
    int  inserted_count    = 0;

    /* Lista para acumular os nomes redundantes encontrados na origem,
     * reutilizando a mesma estrutura generica usada para os nomes do
     * destino. */
    NameList redundant;
    namelist_init(&redundant);

    while (fgets(line, sizeof(line), src)) {
        strip_newline(line);
        if (strlen(line) == 0) continue;

        char *name = extract_first_field(line);

        int found = sequential_search(&existing, name, &total_comparisons);

        if (found) {
            printf("ERRO: nome redundante encontrado: %s\n", name);
            namelist_add(&redundant, name);
        } else {
            fprintf(dst, "%s\n", line);
            namelist_add(&existing, name);
            inserted_count++;
        }

        free(name);
    }

    fclose(src);
    fclose(dst);

    /* ---------------- Geracao do arquivo de estatisticas ------------ */
    /* O arquivo de estatisticas e aberto em modo "append": se o programa
     * for executado varias vezes apontando para o mesmo arquivo (por
     * exemplo, uma vez para cada experimento com 10, 20, 40, 80 e 160
     * nomes), cada execucao adiciona uma nova linha ao final, permitindo
     * que a tabela de resultados cresca ao longo dos experimentos em vez
     * de ser sobrescrita a cada chamada. O cabecalho e escrito apenas
     * quando o arquivo ainda nao existe (primeira execucao).
     *
     * Alem de "nomes_inseridos" e "comparacoes" (exigidos pelo
     * enunciado), sao registradas duas colunas adicionais:
     *   - nomes_redundantes: quantidade de nomes redundantes encontrados
     *     na origem durante a execucao;
     *   - lista_nomes_redundantes: os proprios nomes redundantes,
     *     separados por ';' (nao se usa ',' para nao conflitar com o
     *     delimitador do CSV). Fica vazio se nao houve redundancias. */
    int stats_exists = (access(stats_path, F_OK) == 0);

    FILE *stats = fopen(stats_path, "a");
    if (!stats) {
        fprintf(stderr, "ERRO: nao foi possivel abrir o arquivo de estatisticas '%s'\n", stats_path);
        namelist_free(&existing);
        namelist_free(&redundant);
        return EXIT_FAILURE;
    }
    if (!stats_exists) {
        fprintf(stats, "nomes_inseridos,comparacoes,nomes_redundantes,lista_nomes_redundantes\n");
    }
    fprintf(stats, "%d,%ld,%d,", inserted_count, total_comparisons, redundant.count);
    for (int i = 0; i < redundant.count; i++) {
        fprintf(stats, "%s", redundant.names[i]);
        if (i < redundant.count - 1) {
            fprintf(stats, ";");
        }
    }
    fprintf(stats, "\n");
    fclose(stats);

    namelist_free(&existing);
    namelist_free(&redundant);

    printf("Processo concluido: %d nomes inseridos, %ld comparacoes realizadas.\n",
           inserted_count, total_comparisons);

    return EXIT_SUCCESS;
}