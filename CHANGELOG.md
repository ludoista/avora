# Changelog — AVORA

Todas as mudanças notáveis do projeto **AVORA** (*Analisador Visual de Operações e Rastreamento de Árvores*) são documentadas neste arquivo.

---

## [3.0.0] — 2026-11-19
### Identidade Visual (*Midnight Slate*) & Polimento Gráfico
- Nova paleta **Midnight Slate**: Canvas `#0F111A` com **Dot Grid dinâmico** (`#1E2233`) sincronizado com Pan e Zoom da câmera, editor e painéis em `#161925`, superfícies elevadas `#1F2437` com bordas `#2A3047`.
- Cores semânticas padronizadas: ação primária verde-esmeralda (`#10B981` com texto `#0F111A`), ponteiros esquerdo/`prev` em ciano (`#38BDF8`), direito/`next` em roxo (`#A78BFA`), nó/linha ativa em âmbar (`#F59E0B`) com halo translúcido (`Fade(amber, 0.15f)`).
- Removidas barras de título redundantes para maximizar a área útil de código e visualização.

### Redução de Poluição Visual (*Progressive Disclosure*)
- Removida a caixa externa `Entrada (BUILD_TREE):` — as entradas agora são configuradas diretamente na `main()` do editor (mantendo `BUILD_TREE(...)` como macro auxiliar e o fallback automático caso `main()` seja omitida).
- Rodapé orientado por estado: no modo `EDITING` exibe apenas `[ ▶ Compilar & Iniciar ]`; no modo `RUNNING` troca para `[ ■ Parar ]` e revela `[ ⏭ Passo (Step) ]`, `[ ⏯ Auto-Play ]` e o slider exponencial de `Velocidade`.
- Canvas em 100% da altura com **pílula flutuante de câmera** no canto inferior esquerdo (`[ Auto | - 100% + ]`), incluindo **Auto-Foco por Bounding Box** com `Lerp` que desativa automaticamente ao mover/dar zoom manualmente com o mouse.

### Observabilidade Real (HUD + Terminal Retrátil)
- **HUD Flutuante de Call Stack & Variáveis Locais** (canto superior esquerdo do canvas): consulta `-stack-list-frames` e `-stack-list-variables` via GDB/MI, exibindo a pilha de chamadas e todas as variáveis locais (ponteiros e primitivos `int`, `bool`, `char`, `float`) com destaque em âmbar (`●`) para valores alterados no último passo.
- **Barra de Status & Terminal Retrátil** no rodapé (~28px): expande suavemente ao clicar (ou automaticamente em erros de compilação/execução), com abas para **Diagnóstico & Compilador (GCC/ASan)** e **Saída do Programa (`stdout` / `printf`)** capturada em tempo real em modo *unbuffered*.

### Mascote Blob como Árvore de Decisão Interativa
- Removidos todos os botões `?` da barra superior. Toda a ajuda contextual foi concentrada no **Blob** (canto inferior direito).
- Card flutuante compacto guiado por botões de escolha no modo ocioso (`BUILD_TREE & Listas`, `Modo LeetCode & Vetores`, `Ponteiros & Memória Heap`) e no modo de erro (`Por que isso aconteceu nessa linha?`, `Como corrigir?`, `Ver log bruto no Terminal ▲`).

### Ergonomia do Editor de Código C11
- Seleção por **duplo clique** (palavra inteira) e **triplo clique** (linha inteira).
- Navegação por palavra (`Ctrl + Seta`), *Smart Home*, `Tab` / `Shift + Tab`, scroll horizontal automático com barras de rolagem sutis e histórico completo de **Undo / Redo** (`Ctrl + Z` / `Ctrl + Y` / `Ctrl + Shift + Z`).

### Extensibilidade Multi-Estruturas (18 Templates)
- Suporte nativo a **Listas Encadeadas** (`struct ListNode` com `next`/`prev`, `new_list_node()` e `BUILD_LIST("[...]")`) com layout linear horizontal (**Template 16**).
- Suporte a **Vetores 1D / Busca e Ordenação**: renderização de arrays locais (`int arr[N]` e parâmetros `int*`) com destaque em âmbar para elementos modificados e selos flutuantes para índices (`i`, `j`, `esq`, `meio`, `dir`, `min_idx`, etc.) (**Template 17**).
- Suporte completo a **código C simples** sem alocações na heap via HUD de Call Stack/Variáveis + Terminal `stdout`.

---

## [2.0.0] — 2026-11-09
- Reescrita completa do editor de código com fonte monoespaçada real (`DejaVuSansMono` / `LiberationMono`), medição exata via `MeasureTextEx` (zero *cursor drift*) e engine de seleção por mouse e teclado.
- Tratamento suave de ponteiros recém-alocados "Em Construção" (`[ ? não inic. ]`) sem travar a depuração antes das atribuições de `left`/`right`.
- Expansão para **16 templates** abrangendo LeetCode com `BUILD_TREE`, vetores (`108`, `105`), florestas (`617`, `100`, `226+572`), Buscas (BST/DFS/BFS), BST, Travessias, AVL, Rubro-Negra, Árvore B/2-3 e 3 armadilhas clássicas de memória.
- Instalador Desktop Linux (`install.sh` / `uninstall.sh`) com geração automática de ícones `.png` a partir de `logo.jpg`.

---

## [1.0.0] — 2026-11-06
- Protótipo inicial em C11 + Raylib 5.x + Raygui 4.x + GCC + GDB/MI + AddressSanitizer (ASan).
- Harness de introspecção injetado em tempo de execução (`/tmp/aed_sandbox.c`) com `#line 1 "student_code.c"`, validação de ponteiros via `msync`, prevenção de deadlock ASan/GDB (`detect_leaks=0`) e Watchdog POSIX (`1.5s`) com `setpgid` / `prctl(PR_SET_PDEATHSIG, SIGKILL)`.
