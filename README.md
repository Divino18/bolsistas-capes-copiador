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

## 4. Pontos a considerar na análise

### 4.1 O cabeçalho conta como uma comparação (decisão de projeto)

A implementação trata a linha de cabeçalho do destino como a
**primeira entrada** varrida pela busca sequencial (índice 0 da
`NameList`). Isso significa que, ao inserir o n-ésimo nome distinto em
um destino inicialmente vazio, são realizadas **n** comparações (contra
o cabeçalho + os n-1 nomes já inseridos), não n-1.

Isso resulta em:

$$T(n) = \sum_{i=1}^{n} i = \frac{n(n+1)}{2}$$

em vez de `T(n) = n(n-1)/2` (fórmula que resultaria se o cabeçalho não
fosse contado). Essa escolha foi feita porque é a que reproduz
exatamente os valores do exemplo de saída do enunciado (10→55, 20→210,
40→820, 80→3240, 160→12880). **Documente essa decisão explicitamente
no relatório**, já que a definição textual do enunciado ("comparado com
um registro já existente") admite, em leitura estrita, a interpretação
oposta.

### 4.2 Pressuposto: nomes de entrada são distintos entre si

A análise teórica de pior caso pressupõe que todos os n nomes do
arquivo de origem são distintos (conforme especificado no enunciado).
Sob essa premissa, **nenhuma busca jamais encontra uma correspondência
antecipada** — toda busca percorre a lista completa até o fim, o que é
o que torna a fórmula `T(n) = n(n+1)/2` exata (e não apenas um limite
superior). Se o arquivo de teste contiver nomes repetidos, o número de
comparações observado será **menor** que o previsto pela fórmula (pois
buscas que encontram redundância param mais cedo), e isso deve ser
levado em conta ao montar os arquivos de teste dos experimentos.

### 4.3 Destino deve iniciar vazio a cada experimento

Para que os números coletados correspondam à fórmula teórica
`T(n) = n(n+1)/2`, o arquivo de **destino** precisa começar vazio
(contendo apenas o cabeçalho) antes de cada execução de teste. Se o
destino já contiver registros de uma execução anterior, o número de
comparações observado refletirá o tamanho acumulado do destino, não
apenas o `n` da origem testada.

Já o arquivo de **estatísticas** deve, ao contrário, ser **mantido o
mesmo** entre execuções — é exatamente esse comportamento (modo
append) que permite construir a tabela de resultados experimentais
sem trabalho manual.

### 4.4 Complexidade de espaço (memória)

- A leitura da **origem** é O(1) em memória (streaming, buffer fixo de
  2048 bytes por linha).
- Os nomes do **destino** (incluindo os inseridos durante a própria
  execução) ficam **todos retidos em memória** durante toda a
  execução, em um array que cresce por realocação com dobra de
  capacidade (`capacity *= 2`), garantindo inserção amortizada O(1).
  O consumo total de memória para essa lista é O(m), onde m é o número
  de nomes distintos acumulados no destino — não o tamanho do arquivo
  de origem.
- Isso significa que um arquivo de origem muito grande não é, por si
  só, um problema de memória (ele nunca é carregado por inteiro); o
  gargalo de memória apareceria apenas se o número de nomes
  **distintos** acumulados no destino crescesse além da RAM
  disponível.

### 4.5 Limitações conhecidas

- **Tamanho máximo de linha**: linhas com mais de 2047 caracteres são
  truncadas pelo `fgets` (buffer fixo `MAX_LINE = 2048`). Não é um
  problema para os dados de bolsistas usados no exercício, mas deve
  ser mencionado como limitação de implementação.
- **CSV simplificado**: a extração do campo `Nome` assume que ele
  nunca contém vírgulas (não há suporte a campos entre aspas com
  vírgula interna, como exigido pela RFC 4180 completa). Adequado ao
  formato dos dados fornecidos.
- **Sem normalização de espaços**: o campo `Nome` não passa por
  `trim`; espaços extras ao redor do nome tornariam dois registros
  "iguais" em tratados como distintos pelo `strcmp`.
- **Colunas extras no arquivo de estatísticas**: `nomes_redundantes` e
  `lista_nomes_redundantes` são uma extensão além do que o enunciado
  pede explicitamente (que exige apenas `nomes_inseridos` e
  `comparacoes`). A análise teórica e a demonstração por indução usam
  apenas essas duas colunas originais; as colunas extras servem como
  rastreabilidade/auditoria adicional.

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
