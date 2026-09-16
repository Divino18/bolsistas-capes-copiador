# Copiador de Bolsistas CAPES — Análise de Complexidade

Programa em C que copia registros de bolsistas de um arquivo CSV de origem
para um arquivo CSV de destino, evitando duplicidade de nomes, e instrumenta
a execução para permitir a validação experimental de sua complexidade
temporal.

---

## 1. Compilação

```bash
gcc -Wall -Wextra -O2 -o copiador copiador.c
```

Não há dependências externas — apenas a biblioteca padrão do C
(`stdio.h`, `stdlib.h`, `string.h`, `unistd.h`).

---

## 2. Execução

```bash
./copiador <origem.csv> <destino.csv> <estatisticas.csv>
```

| Argumento | Significado |
|---|---|
| `origem.csv` | Arquivo CSV com os registros a importar (ex: `bolsistas-2025.csv`) |
| `destino.csv` | Arquivo CSV de cadastro consolidado, que recebe os registros novos |
| `estatisticas.csv` | Arquivo CSV onde são registradas as métricas de cada execução |

Exemplo:

```bash
./copiador bolsistas-2025.csv cadastro.csv estatisticas.csv
```

Formato de cada linha dos CSVs de entrada/saída de dados:

```
Nome,Modalidade,Nivel,Agencia
```

---

## 3. Como o programa funciona (passo a passo)

### 3.1 Carregamento do destino em memória

Antes de processar a origem, o programa lê o arquivo de destino
**inteiro** (se ele já existir) e guarda em memória apenas o campo
`Nome` de cada linha, incluindo o **cabeçalho**, em uma lista dinâmica
(`NameList`). Essa lista é o que permite comparar rapidamente cada novo
nome contra todos os nomes já conhecidos, sem reabrir o arquivo em disco
a cada comparação.

Se o destino ainda não existir, a lista é inicializada apenas com o
nome `"Nome"` (representando o cabeçalho que será criado), simulando o
mesmo comportamento.

### 3.2 Leitura da origem (streaming)

O arquivo de origem **não é carregado por inteiro** na memória. Ele é
lido linha a linha com `fgets`, usando um buffer fixo de 2048 bytes que
é reaproveitado a cada iteração. Isso mantém o consumo de memória da
leitura da origem constante (O(1)), independente do tamanho do arquivo.
O cabeçalho da origem é lido e descartado antes do laço principal.

### 3.3 Verificação de redundância (busca sequencial)

Para cada nome lido da origem, o programa executa uma **busca
sequencial** (`sequential_search`) percorrendo a lista de nomes do
destino (começando pelo cabeçalho, índice 0) e comparando com
`strcmp`. A busca:

- incrementa o contador global de comparações **a cada `strcmp`
  executado**, independentemente do resultado;
- para assim que encontra uma correspondência (nome redundante), **sem
  continuar comparando o restante da lista**.

### 3.4 Decisão de cópia

- **Nome não encontrado** → a linha completa (todos os campos
  originais) é copiada para o destino, e o nome é adicionado à lista em
  memória para participar das buscas seguintes.
- **Nome encontrado (redundante)** → a linha **não** é copiada.
  É impressa em `stdout` a mensagem:
  ```
  ERRO: nome redundante encontrado: <nome>
  ```
  A execução **continua normalmente** (o programa não é interrompido).
  O nome também é adicionado a uma segunda lista em memória, dedicada
  a nomes redundantes.

### 3.5 Geração do arquivo de estatísticas

Ao final da execução, uma linha é **anexada** (modo `append`, não
sobrescreve) ao arquivo de estatísticas, contendo:

```
nomes_inseridos,comparacoes,nomes_redundantes,lista_nomes_redundantes
```

- `nomes_inseridos`: quantidade de registros efetivamente copiados
  para o destino nesta execução;
- `comparacoes`: número total de comparações (`strcmp`) realizadas
  nesta execução;
- `nomes_redundantes`: quantidade de nomes rejeitados por já existirem
  no destino;
- `lista_nomes_redundantes`: os próprios nomes redundantes, separados
  por `;` (não se usa `,` para não conflitar com o delimitador do
  CSV). Fica vazio se não houve redundâncias nesta execução.

O cabeçalho dessa tabela só é escrito na **primeira** vez que o arquivo
é criado — chamadas seguintes ao programa, apontando para o mesmo
arquivo de estatísticas, apenas acrescentam novas linhas. Isso permite
rodar vários experimentos (ex: 10, 20, 40, 80, 160 nomes) sobre o mesmo
`estatisticas.csv` e obter uma tabela consolidada automaticamente.

---

## 5. Roteiro sugerido para os experimentos

1. Gere arquivos de origem com **n nomes distintos** para
   n = 10, 20, 40, 80, 160 (nomes únicos, sem repetição).
2. Para cada valor de n:
   - remova/recrie o arquivo de **destino** vazio (ou use um nome de
     arquivo de destino diferente por experimento);
   - execute `./copiador origem_n.csv destino.csv estatisticas.csv`,
     sempre apontando para o **mesmo** `estatisticas.csv`.
3. Ao final dos 5 experimentos, `estatisticas.csv` conterá uma tabela
   com 5 linhas (mais o cabeçalho), pronta para ser comparada com os
   valores previstos por `T(n) = n(n+1)/2` na Parte 3 do relatório.
4. Para cada linha, calcule a diferença entre o valor observado
   (coluna `comparacoes`) e o valor previsto pela fórmula teórica, e
   registre isso na tabela de validação experimental.
