<p align="center">
  <img src="logo.jpg" alt="AVORA Logo" width="180" style="border-radius: 20px;" />
</p>

<h1 align="center">AVORA v3.0.1</h1>
<h3 align="center">Analisador Visual de Operações e Rastreamento de Árvores</h3>

<p align="center">
  <b>Depurador Visual Interativo para Algoritmos e Estruturas de Dados em C11 (Linux / POSIX)</b><br/>
  <i>Motor em Tempo Real com GCC + GDB/MI + AddressSanitizer (ASan) + Raylib 5.x / Raygui 4.x</i>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Vers%C3%A3o-3.0.1-10B981?style=flat-square" alt="v3.0.1" />
  <img src="https://img.shields.io/badge/Linguagem-C11%20(POSIX)-38BDF8?style=flat-square" alt="C11" />
  <img src="https://img.shields.io/badge/Plataforma-Linux%20x86__64-1F2437?style=flat-square" alt="Linux" />
  <img src="https://img.shields.io/badge/Backend-GDB%2FMI%20%2B%20ASan-A78BFA?style=flat-square" alt="GDB/MI + ASan" />
  <img src="https://img.shields.io/badge/UFPel-Ci%C3%AAncia%20da%20Computa%C3%A7%C3%A3o-0F111A?style=flat-square" alt="UFPel" />
</p>

---

## 🎓 Contexto Acadêmico e Motivação

O **AVORA** (*Analisador Visual de Operações e Rastreamento de Árvores*) é um projeto pessoal, de viés acadêmico, desenvolvido por **Filipe de Melo Pereira**, graduando do curso de **Ciência da Computação**.

### Por que o AVORA foi criado?
No estudo de **Algoritmos e Estruturas de Dados** utilizando a linguagem **C**, uma das maiores dificuldades enfrentadas pelos estudantes é a lacuna cognitiva entre o código escrito no editor e o estado real da memória *heap* e da pilha de execução (*call stack*):

1. **Invisibilidade das Mutações de Ponteiros:** Quando um aluno implementa uma rotação dupla em uma Árvore AVL, uma recoloração em uma Árvore Rubro-Negra, uma cisão (*split*) em uma Árvore B/2-3, uma inversão de Lista Encadeada ou uma remoção com sucessor em-ordem em uma BST, reatribuições como `pai->left = filho->right` acontecem silenciosamente na memória. Um único ponteiro reatribuído na ordem errada pode desconectar uma subárvore inteira ou criar um ciclo infinito.
2. **Erros Silenciosos de Memória em C:** Diferente de linguagens gerenciadas, em C a função `malloc()` não inicializa a memória com zero. Esquecer de atribuir `novo->left = NULL;` ou desreferenciar um ponteiro nulo (`raiz->val` quando `raiz == NULL`) resulta em falhas abruptas (`SIGSEGV` / *Segmentation Fault*) cujas mensagens no terminal raramente são claras para quem está aprendendo.
3. **Observabilidade de Recursão, Vetores e Código Real:** Plataformas como o LeetCode permitem testar algoritmos em árvores (`struct TreeNode`), listas (`struct ListNode`) e vetores, mas não oferecem uma visualização passo a passo da pilha de chamadas, das variáveis locais escalares/ponteiros mudando a cada instrução e da saída `printf` simultânea.

O **AVORA** nasceu para solucionar esse problema: em vez de simular estruturas de forma estática, ele **compila o código C real do estudante com `gcc`** e o executa dentro de um processo **GDB via Machine Interface (GDB/MI)** combinado com **AddressSanitizer (ASan)** e um *Harness de Introspecção em Tempo de Execução*. A cada passo (`Step` ou `Auto-Play`), a memória real do programa é inspecionada e renderizada a **60 FPS** em uma interface moderna (**Midnight Slate**) com animações suaves, HUD de observabilidade, terminal retrátil e o auxílio do mascote interativo **Blob**.

---

## 📥 Como Baixar e Instalar

> **Pré-requisito de Execução:** Como o AVORA compila e depura seu código C em tempo real usando o compilador e o depurador nativos do sistema, certifique-se de ter o **`gcc`** e o **`gdb`** instalados na sua distribuição Linux:
> - **Ubuntu / Debian / Mint / Pop!_OS:** `sudo apt update && sudo apt install gcc gdb`
> - **Fedora:** `sudo dnf install gcc gdb`
> - **Arch Linux / Manjaro:** `sudo pacman -S gcc gdb`

---

### Opção 1: Baixar a Versão Pronta para Uso (Aba *Releases* — Recomendado)

Se você quer apenas baixar e usar o aplicativo imediatamente (sem precisar compilar o código-fonte do AVORA nem instalar bibliotecas gráficas extras):

1. Acesse a aba **[Releases](https://github.com/ludoista/avora/releases)** aqui no GitHub.
2. Baixe o arquivo **`AVORA-v3.0.0-Linux-x86_64-Release.zip`**.
3. Extraia o arquivo `.zip` e escolha como deseja usar:

```bash
unzip AVORA-v3.0.0-Linux-x86_64-Release.zip
cd AVORA-v3.0.0-Linux-x86_64

# Para abrir imediatamente sem instalar no sistema:
./run.sh

# OU para instalar o aplicativo e o ícone no menu de aplicativos do seu Linux:
./install.sh
```

Para remover a instalação do menu de aplicativos a qualquer momento, basta rodar `./uninstall.sh`.

---

### Opção 2: Clonar o Repositório e Compilar do Código-Fonte

Se você deseja explorar o código-fonte, modificar o projeto ou compilar manualmente a partir deste repositório:

#### 1. Instale as dependências de desenvolvimento (`raylib`, `gcc`, `gdb`, `make`)
- **Ubuntu / Debian:**
  ```bash
  sudo apt update && sudo apt install build-essential gcc gdb libraylib-dev
  ```
- **Fedora:**
  ```bash
  sudo dnf install gcc gdb make raylib-devel
  ```
- **Arch Linux:**
  ```bash
  sudo pacman -S base-devel gcc gdb raylib
  ```

#### 2. Clone o repositório e instale (ou execute diretamente)
```bash
git clone https://github.com/ludoista/avora.git
cd avora

# Compila, gera os ícones a partir da logo.jpg e instala no menu de aplicativos:
./install.sh

# Ou apenas compile e rode localmente no terminal:
make
./aed_tree_debugger
```

---

## ✨ Principais Funcionalidades (v3.0.0)

### 1. Identidade Visual *Midnight Slate* & Interface Orientada a Estados (*Progressive Disclosure*)
- **Canvas em 100% da Altura:** Fundo `#0F111A` com **Dot Grid dinâmico** (`#1E2233`) sincronizado com Pan e Zoom, e nós estilizados em `#1F2437` com halo âmbar (`#F59E0B`) no nó em foco.
- **Sem Poluição Visual:** Removidas barras de título redundantes e entradas externas. No modo **Edição**, o rodapé exibe apenas `[ ▶ Compilar & Iniciar ]`; no modo **Execução**, revela `[ ■ Parar ]`, `[ ⏭ Passo (Step) ]`, `[ ⏯ Auto-Play ]` e o slider exponencial de `Velocidade` (`0.05x` a `100x`).
- **Pílula Flutuante de Câmera (`[ Auto | - 100% + ]`):** No canto inferior esquerdo do canvas, com toggle de **Auto-Foco** por *bounding box* que enquadra a estrutura suavemente (`Lerp`) a cada passo e desarma automaticamente ao detectar arraste ou scroll manual do usuário.

### 2. Observabilidade Real: HUD de Call Stack/Variáveis + Terminal Retrátil
- **HUD Flutuante (Canto Superior Esquerdo):** Exibe durante a depuração a **Pilha de Chamadas (`-stack-list-frames`)** e todas as **variáveis locais** do frame atual (ponteiros `0x...`/`NULL` e primitivos `int`, `bool`, `char`, `float`), destacando em âmbar (`●`) toda variável cujo valor mudou no último passo.
- **Barra de Status & Terminal Retrátil (Rodapé):** Barra enxuta de status que desliza para cima ao clicar (ou automaticamente em falhas do compilador/execução), alternando entre:
  - **Diagnóstico & Compilador (`GCC` / `ASan`):** Explicação didática + saída literal multilinha do compilador com linha exata.
  - **Saída do Programa (`stdout` / `printf`):** Captura em tempo real e sem *buffering* de qualquer `printf`/`puts` executado pelo código do estudante.

### 3. 18 Templates Interativos (Árvores, Florestas, Listas Encadeadas e Vetores)
O seletor no topo do editor (acompanhado do botão `[ ↺ Resetar ]`) oferece **18 exemplos prontos e comentados**:

| # | Categoria | Template | Destaque Didático |
| :-: | :--- | :--- | :--- |
| **0** | Scratchpad | **Vazio (Scratchpad Livre)** | Esqueleto limpo para criar sua própria árvore ou algoritmo C |
| **1** | LeetCode (BST) | **538. Convert BST to Greater Tree** | Travessia em-ordem reversa com `BUILD_TREE` |
| **2** | LeetCode (Vetor) | **108. Vetor Ordenado $\rightarrow$ BST** | `main()` com vetor `int nums[]`, divisão e conquista e `malloc()` |
| **3** | LeetCode (2 Vetores)| **105. Pré & Em-Ordem $\rightarrow$ Árvore** | Reconstrução de árvore a partir de 2 vetores (`preorder` e `inorder`) |
| **4** | LeetCode (Floresta) | **617. Merge Two Binary Trees** | Duas árvores (`t1` e `t2`) lado a lado sendo mescladas em tempo real |
| **5** | LeetCode (Floresta) | **100. Same Tree** | Comparação estrutural simultânea de duas árvores no canvas |
| **6** | LeetCode (Floresta) | **226. Invert Tree + 572. Subtree** | Três árvores na floresta: inversão espelhada e verificação de subárvore |
| **7** | Buscas | **Binária (BST), DFS & BFS** | Comparação prática entre busca em BST, profundidade e largura |
| **8** | BST Clássica | **Inserção & Remoção (3 Casos)** | Remoção de nó folha, 1 filho e 2 filhos (sucessor em-ordem) |
| **9** | Travessias | **Pré-Ordem, Em-Ordem e Pós-Ordem** | Rastreamento visual da pilha de chamadas recursivas |
| **10** | Balanceamento | **Árvore AVL** | Fator de balanceamento e rotações simples/duplas (`LL`, `RR`, `LR`, `RL`) |
| **11** | Balanceamento | **Árvore Rubro-Negra (Red-Black)** | Rotações à esquerda/direita e *flip* de cores |
| **12** | Multi-Chaves | **Árvore B / 2-3 (Cisão / Split)** | Simulação visual de cisão (*split*) e promoção de chave mediana |
| **13** | Armadilha 1 | **Ponteiro Sem `NULL` (Lixo)** | Demonstra o que ocorre ao esquecer `left = right = NULL` após `malloc()` |
| **14** | Armadilha 2 | **Ciclo Estrutural (Rotação)** | Demonstra a detecção imediata de laço fechado (`pai <-> filho`) |
| **15** | Armadilha 3 | **Segfault (Desreferenciar `NULL`)** | Demonstra o acesso inválido `raiz->val` sem verificar `if (raiz == NULL)` |
| **16** | Lista Encadeada | **Inversão & Ponteiros (`next`/`prev`)** | `struct ListNode` + `BUILD_LIST("[10, 20, 30, 40, 50]")` com layout linear |
| **17** | Vetores & Busca | **Ordenação (Selection) & Busca Binária** | Células de vetor no Canvas com selos de índice (`i`, `j`, `esq`, `meio`, `dir`) + `printf` |

### 4. Mascote Pedagógico **Blob** (Árvore de Decisão Interativa)
No canto inferior direito vive o **Blob**, concentrando toda a ajuda contextual sem poluir o topo da tela:
- **Modo Exploratório:** Ao clicar no Blob, abre um card flutuante compacto com botões de decisão clicáveis (`Como usar BUILD_TREE & Listas`, `Modo LeetCode & Vetores`, `Guia Visual de Ponteiros & Memória Heap`).
- **Modo Erro:** Acionado automaticamente em falhas de compilação, `SIGSEGV`, memória não inicializada (`0xBE`) ou ciclos, oferecendo botões contextuais como *"Por que isso aconteceu nessa linha?"*, *"Como corrigir?"* e *"Ver log bruto no Terminal ▲"*.

### 5. Ergonomia Moderna do Editor de Código C11
- **Seleção com Mouse:** Duplo clique seleciona a palavra inteira (`isalnum` ou `_`); triplo clique seleciona a linha inteira.
- **Atalhos & Navegação:** `Ctrl+Z` / `Ctrl+Y` (*Undo/Redo*), `Ctrl+A`, `Ctrl+C/X/V`, `Ctrl+Seta` (pular palavra), *Smart Home* / `End`, `Tab` / `Shift+Tab`, e scroll horizontal/vertical automático acompanhando o cursor.

---

## 🗂️ Organização do Código-Fonte e Arquitetura

O código-fonte do **AVORA** é escrito em **C11 puro (`-std=c11 -D_GNU_SOURCE`)** para Linux, sem dependências externas além de `libraylib`, `libm` e `libc`:

```text
avora/
├── include/
│   ├── visualizer.h        # Contrato central: máquinas de estado, TreeSnapshot, VisualArray,
│   │                       # StackFrameInfo, LocalScalarVar, Undo/Redo e árvore do Blob.
│   └── raygui.h            # Implementação customizada do Raygui 4.x com tema Midnight Slate
│                           # e GuiPrimaryButton (#10B981).
├── src/
│   ├── main.c              # Ponto de entrada, janela OpenGL/Raylib a 60 FPS e loop principal.
│   ├── ipc_gdb.c           # IPC POSIX não-bloqueante (fork/pipe/poll/setpgid/prctl), Harness
│   │                       # injetado (/tmp/aed_sandbox.c), captura unbuffered de stdout,
│   │                       # suporte a TreeNode/ListNode/Vetores, Call Stack e 18 templates.
│   ├── layout_renderer.c   # Layout de Árvores, Florestas, Listas Encadeadas e Vetores 1D,
│   │                       # Dot Grid dinâmico, Pílula de Câmera Auto-Foco, HUD de Variáveis
│   │                       # & Pilha e árvore de decisão animada do Blob.
│   ├── ui_workspace.c      # Editor C11 (duplo/triplo clique, Undo/Redo, Ctrl+Seta, syntax
│   │                       # highlighting), Progressive Disclosure e Terminal Retrátil.
│   └── diagnostics.c       # Tradutor pedagógico de erros GCC, GDB e AddressSanitizer.
├── logo.jpg                # Arte oficial do AVORA (ícone da janela e do instalador Linux).
├── Makefile                # Build padrão C11 (-Wall -Wextra -O2 -g3).
├── install.sh              # Instalador desktop Linux (compila, gera ícones PNG e .desktop).
├── uninstall.sh            # Desinstalador limpo dos arquivos de usuário.
├── CHANGELOG.md            # Histórico de versões (v1.0.0 -> v2.0.0 -> v3.0.0).
├── LICENSE                 # Licença do projeto.
└── README.md               # Documentação principal do projeto.
```

---

## 👨‍💻 Autor

- **Autor:** **Filipe de Melo Pereira**
- **Curso:** Ciência da Computação
- **Instituição:** **Universidade Federal de Pelotas (UFPel)**

---

<p align="center">
  Desenvolvido com dedicação para tornar o estudo de <b>Algoritmos e Estruturas de Dados</b> mais visual, intuitivo e acessível.
</p>
