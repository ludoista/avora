# Changelog — AVORA

Todas as mudanças notáveis do projeto **AVORA** (*Analisador Visual de Operações e Rastreamento de Árvores*) são documentadas neste arquivo.

---

## [3.3.1] — 2026-11-19 (Hotfix de Layout & Responsividade da UI)
- **Alinhamento pela direita e limites seguros nas Configurações (`draw_settings_modal`)**:
  - Modal ampliado para `900px` (`fminf(900.0f, sw - 32.0f)`) para comportar a fonte monoespaçada com folga.
  - Aba *Editor*: controles de tamanho da fonte (`-`, `16 px`, `+`, `Padrão (16)`) e seleção de indentação (`2 Espaços`, `4 Espaços`) ancorados dentro da largura útil `cw`, sem vazar pela borda direita.
  - Aba *Aparência*: botões segmentados do fundo (`Dot Grid`, `Linhas`, `Liso`) e labels das checkboxes encurtados para caberem sem corte.
  - Aba *Depurador*: slider de velocidade inicial e `Reset 1.0x` proporcionais a `cw`, e checkboxes com texto conciso.
- **Modal de Atalhos (`draw_shortcuts_modal`)**: ampliado para `880px` com duas colunas balanceadas e `BeginScissorMode` de proteção.
- **Menus Dropdown da Barra Superior**: largura ampliada (`fminf(380.0f, sw - menu_x - 8.0f)`) para eliminar sobreposição entre nomes de ação e atalhos (`Shift+F10`, `Ctrl+,`).

---

## [3.3.0] — 2026-11-19
### Modo Apresentação / Palco Completo (Controle via Setas & Passador Bluetooth)
- **Navegação por Setas e Controle Remoto Bluetooth (`Ctrl+Shift+P`)**:
  - Ao ativar o Modo Apresentação, o foco de digitação é liberado para que as teclas **`→` / `PageDown` / `Espaço`** avancem passos (ou compilem e iniciem quando ocioso), **`←` / `PageUp`** voltem passos no *Time-Travel*, **`↑` / `↓`** rolem suavemente o código no editor sem mouse, **`[` / `]`** troquem de template e **`Esc`** saia do modo palco.
  - Auto-scroll inteligente do editor mantendo a linha em execução centralizada a cada passo.
- **Palco Concatenado & Nós Ampliados**:
  - Nós do Canvas maiores (`30px` de raio, fonte `22px`, badges `15px`) e Auto-Foco com escala ampliada para leitura clara em projetores.
  - **Console & Diagnóstico de Palco sobre o Canvas (`draw_presentation_stage_overlay`)**: card translúcido e espaçado na base do Canvas que concatena o estado/diagnóstico da linha atual e as últimas linhas de `stdout` (`printf`) / GCC diretamente na tela de nós, sem precisar abrir a gaveta do terminal retrátil.

### Reorganização Sequencial dos 24 Templates & Nomenclatura Universal
- **Trilha Pedagógica Progressiva (`0` a `23`)**:
  - **0:** Scratchpad Livre
  - **1–4 (Estruturas Lineares):** Lista Encadeada Simples (`BUILD_LIST`), Lista Duplamente Encadeada com Sentinela, Pilha (`Stack` LIFO) e Fila Circular (`Queue` FIFO)
  - **5–16 (Árvores, LeetCodes & Balanceamento):** Travessias, Buscas (`BST`/`DFS`/`BFS`), BST (Inserção & Remoção), LeetCodes `538`, `108`, `105`, `617`, `100`, `226+572`, Árvore AVL, Árvore Rubro-Negra e Árvore B / 2-3
  - **17–18 (Ordenação & Busca):** Selection Sort + Busca Binária, QuickSort & Partição de Lomuto
  - **19–20 (Estruturas Avançadas):** Binary Heap (Max-Heap) & HeapSort, Tabela Hash (Encadeamento Separado)
  - **21–23 (Armadilhas de Memória):** Ponteiro Sem `NULL` (Lixo `0xBE`), Ciclo Estrutural e Segfault (`NULL`)
- **Nomenclatura Limpa**: removidos prefixos específicos de disciplina (`"AED1"` / `"Modo Sala de Aula"`) em favor de `"Templates Personalizados por Pasta (Biblioteca Local .c)"`.

### Detecção Correta de Ciclos em Listas Duplas & Tabela Hash
- **Diferenciação entre Vínculo Bidirecional (`prev <-> next`) e Ciclo Real**:
  - `__aed_can_reach()` e `__aed_dump_tree()` no *harness* agora tratam o ponteiro `left` (`prev`) de nós `struct ListNode` (`LIST_NODE`) como referência reversa ao predecessor, verificando alcançabilidade apenas pela cadeia `next` (`right`).
  - Na descoberta de raízes (`refresh_runtime_snapshot`), arestas `prev` não marcam o nó cabeça como filho, permitindo que **Tabela Hash com Encadeamento** e **Lista Duplamente Encadeada** executem até `STATE_FINISHED` sem falso positivo de ciclo, mantendo a detecção imediata para ciclos reais.

### Polimento Visual de Tema, Nitidez 1:1 & Layout de Modais/Menus
- **Badge `[ ? não inic. ]` integrada ao Tema Ativo**: `draw_soft_uninit_pointer()` agora utiliza as cores da paleta ativa (`pal->bg_elevated`, `pal->accent_warning`, `pal->text_secondary`), mantendo contraste tanto em temas escuros quanto no *Serika Light*.
- **Nitidez 1:1 nas Checkboxes**: `draw_checkbox_row()` redesenhada com checkmark vetorial (`DrawLineEx`) e texto exatamente na resolução nativa do atlas (`13.0f` com `floorf`, eliminando borrão sub-pixel).
- **Zero Cortes ou Overlaps**: ampliadas as larguras e espaçamentos do modal de Configurações (`860px`), modal de Atalhos (`780px`) e menus dropdown superiores (`340px`).

---

## [3.2.0] — 2026-11-19
### Polimento Visual ("Responsividade iOS + Click Crisp Indie") & Áudio Procedural (`src/audio.c`)
- **Física Exponencial Independente de Framerate (`avora_exp_decay`)**: todas as interpolações de câmera (Pan/Zoom/Auto-Foco), transições de nós no Canvas 2D, rolagem do editor/terminal, HUD e mascote Blob agora usam decaimento exponencial contínuo (`1.0f - expf(-speed * dt)`), 100% interrompível e respeitando `reduce_animations`.
- **Micro-feedback Tátil "Indie Crisp"**:
  - Deslocamento vertical sutil (`1.5px`) e realce nítido de borda no `IsMouseButtonDown` nos botões primários e secundários (`GuiPrimaryButton`, `GuiButton`).
  - Flash curto de borda (`~150ms`) em nós recém-alocados ou modificados (rotações AVL, splits de Árvore B, recolorações Rubro-Negra, mutações em listas e vetores).
  - Micro-shake sutil (`2–3px` por `~150ms`) + flash vermelho discreto no canvas em erros de compilação, `SIGSEGV` ou armadilhas do AddressSanitizer.
- **Motor de Áudio Procedural em Memória (`src/audio.c`)**:
  - Síntese procedural leve na inicialização (zero arquivos `.wav` externos) gerando 5 efeitos sutis: *Tick* mecânico em `Step`, *Pop* harmônico (com variação de pitch conforme a quantidade de nós) ao alocar/mutar nós, *Chime* ascendente ao compilar com sucesso, *Thud* grave em falhas/traps e *Click* discreto na UI.
  - 100% *fail-safe* em ambientes Linux sem servidor de áudio ativo, com botão rápido de **Mudo (`♪ SFX` / `✕ Mudo`)** na barra superior e controle de volume (`audio_enabled`, `audio_volume`) persistido em `~/.config/avora/config.ini`.

### Terminal Unificado + `stdin` Opcional & Sugestão Contextual do Blob
- **Aba `Terminal` Unificada**: painel retrátil limpo com apenas 2 abas (`Diagnóstico & Compilador (GCC/ASan)` e `Terminal`).
- **`stdin` Opcional (`enable_stdin` em `Configurações → Depurador`)**:
  - Desativado por padrão (`false`) para manter zero poluição visual no modo LeetCode/`BUILD_TREE`.
  - Quando ativado (`true`), divide a aba `Terminal` em **Entrada (`stdin`)** editável e **Saída (`stdout`)**, gravando `/tmp/aed_stdin.txt` e redirecionando o `stdin` do processo depurado com `EOF` imediato caso vazio (nunca bloqueando o Watchdog POSIX de `1.5s`).
- **Detecção Inteligente do Blob (`BLOB_HINT_STDIN_DETECTED`)**: ao compilar/executar código contendo `scanf`, `fgets`, `getchar` ou `getline` com `enable_stdin == false`, o Blob sugere ativar e abrir o campo `stdin` em **1 clique**.

### Expansão da Biblioteca Acadêmica (24 Templates Embutidos de AED I & AED II)
- Adicionados **6 novos templates clássicos** de disciplinas de Algoritmos e Estruturas de Dados (`PRESET_COUNT = 24`):
  - **18. Pilha (Stack):** Expressões & Parênteses Balanceados (`push`/`pop` em `struct ListNode` + vetor de rastreamento).
  - **19. Fila Circular (Queue):** Buffer Modular com `head`, `tail`, `count` e aritmética `(tail + 1) % CAP`.
  - **20. Lista Duplamente Encadeada com Nó Sentinela:** Inserção e remoção $O(1)$ com ponteiros `prev` e `next`.
  - **21. QuickSort & Partição de Lomuto:** Rastreamento visual de `pivo`, `i`, `j` e recursion stack no HUD.
  - **22. Binary Heap (Max-Heap) & HeapSort:** `sift_down` in-place + projeção visual da árvore implícita via `BUILD_TREE`.
  - **23. Tabela Hash com Encadeamento Separado:** Tratamento de colisões com listas encadeadas e inspeção de buckets.

### Internacionalização (`src/i18n.c`) & Diagnóstico do Sistema
- **Seletor de Idioma em Tempo Real (`PT-BR`, `EN-US`, `ES`)**: módulo `src/i18n.c` com tradução imediata dos menus, botões principais, abas do terminal, HUD, configurações e diagnósticos, persistido em `config.ini` (`language`).
- **Modal `Diagnóstico do Sistema & Suporte` (`Menu Ajuda`)**: verifica automaticamente as versões instaladas do `gcc` e `gdb`, suporte a AddressSanitizer, status do subsistema de áudio e caminhos de configuração, com botão para copiar um relatório Markdown completo para a área de transferência.

### Ergonomia Avançada de Depuração: Time-Travel Visual (`Shift+F10`) & Breakpoints no Gutter (`F9`)
- **Voltar Passo / Visual Time-Travel (`[ ⏮ Voltar ]` / `Shift+F10`)**: ring buffer em memória dos últimos **128 snapshots** (`TreeSnapshot` + linha ativa + Call Stack + variáveis locais), permitindo retroceder e re-avançar passos instantaneamente com animação suave sem depender de `reverse-step` do GDB.
- **Breakpoints Clicáveis no Gutter & Continuar (`[ ▶ Continuar ]` / `F9`)**: clique diretamente no número da linha no gutter do editor para ativar/desativar breakpoints (`●` vermelho), sincronizados via `-break-insert student_code.c:<linha>` com o GDB, além de item no menu `Executar` para limpar todos os breakpoints.

---

## [3.1.0] — 2026-11-19
### Experiência Completa de Aplicativo Desktop (Menu Bar, Atalhos & Janela)
- **Menu Bar superior minimalista (`26px`)**: abas clássicas `Arquivo`, `Editar`, `Executar`, `Janela` e `Ajuda` com menus dropdown integrados ao tema ativo, indicador do arquivo/tema atual e acesso em 1 clique a `⚙ Configurações`.
- **Polimento da Janela OS**:
  - Título minimalista: `"AVORA"` (ou `"AVORA — arquivo.c ●"` quando há arquivo aberto ou alterações não salvas).
  - Correção do ícone da janela no X11/Wayland/GLFW (`SetWindowIcon` com conversão garantida para RGBA 32-bit `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` a partir de `~/.local/share/avora/avora.png`, `/usr/local/share/avora/avora.png` ou `./logo.jpg`).
  - Persistência automática de geometria da janela (`window_w`, `window_h`, estado maximizado) e posição do splitter vertical ao fechar.
- **Atalhos Globais de Teclado**: `F5` (Compilar & Iniciar), `F10` (Passo/Step), `F8` (Auto-Play/Pausar), `Shift+F5` (Parar), `F11` (Tela Cheia), `F1` (Atalhos), `Ctrl+,` (Configurações), `Ctrl+N` (Novo), `Ctrl+O` (Abrir `.c`), `Ctrl+S` / `Ctrl+Shift+S` (Salvar / Salvar Como), `Ctrl+E` (Exportar PNG), `Ctrl+R` (Resetar Template), `Ctrl+\`` / `Ctrl+J` (Terminal), `Ctrl+Shift+P` (Modo Apresentação), `Ctrl++` / `Ctrl+-` / `Ctrl+0` (Zoom da fonte do editor) e `Ctrl+Q` (Sair).

### Sistema de Configurações Persistentes (`~/.config/avora/config.ini`) & 7 Temas (Estilo Monkeytype)
- **Modal de Configurações (`Ctrl+,`)** organizado em 4 abas laterais (`Aparência & Temas`, `Editor de Código`, `Depurador (GCC/GDB)`, `Templates & Janela`) com salvamento instantâneo e botão `↺ Restaurar Padrões`.
- **7 Temas de Cores inspirados no Monkeytype** aplicados em tempo real no Editor, RayGUI, Canvas 2D, Nós, Vetores, HUD, Terminal e Blob:
  1. **Midnight Slate** *(padrão original do AVORA)*
  2. **Serika Dark** *(`#323437` / `#E2B714` / `#D1D0C5`)*
  3. **Catppuccin Mocha** *(`#1E1E2E` / `#313244` / `#CBA6F7`)*
  4. **Nord** *(`#2E3440` / `#3B4252` / `#88C0D0`)*
  5. **Gruvbox Dark** *(`#282828` / `#3C3836` / `#FABD2F`)*
  6. **Dracula** *(`#282A36` / `#44475A` / `#BD93F9`)*
  7. **Serika Light (Paper)** *(tema claro de alto contraste para projetores de sala de aula e relatórios)*
- **Opções de Visualização, Editor & Depurador**:
  - Estilo do fundo do Canvas (`Dot Grid`, `Grade de Linhas`, `Fundo Liso`), toggle de endereços de memória (`0x...`) nos nós, toggle do HUD, toggle do Mascote Blob, redução de animações e **Modo Apresentação / Projetor**.
  - Recarregamento dinâmico 1:1 do atlas da fonte do editor (`12px` a `24px`), largura do Tab (`2` ou `4` espaços), números de linha, destaque da linha atual, auto-indentação e restauração da última sessão.
  - Toggle do **AddressSanitizer (ASan)**, timeout configurável do **Watchdog POSIX** (`1.5s`, `3.0s`, `5.0s`, `10.0s`), velocidade inicial do Auto-Play e abertura automática do terminal em `stdout`.

### Templates Dinâmicos por Pasta (Modo Sala de Aula / AED1) & Operações de Arquivo
- **Templates dinâmicos baseados em diretório** (`~/.config/avora/templates` ou qualquer pasta escolhida):
  - Escaneia automaticamente arquivos `.c` (e subpastas como `[Aula01] exemplo.c`) e popula o seletor de templates com suporte a rolagem para até 64 itens.
  - 3 modos de exibição: **Embutidos + Minha Pasta**, **Apenas Minha Pasta** (ideal para professores deixarem apenas os exemplos da aula) e **Apenas Embutidos (18)**.
  - Botões utilitários para **Escolher Pasta**, **Abrir Pasta no Gerenciador (`xdg-open`)**, **Exportar os 18 Templates Padrão para a Pasta** e **Recarregar Lista**.
- **Operações completas de Arquivo**: Abrir `.c`, lista de **Arquivos Recentes**, Salvar, Salvar Como, Salvar na Pasta de Templates, proteção contra descarte acidental de alterações não salvas e **Exportar Canvas como PNG**.

---

## [3.0.1] — 2026-11-19 (Hotfix)
### Nitidez de Fontes (Full HD / 1080p) & Layout de Botões
- **Renderização de texto 1:1 na UI + Mipmaps no Canvas**: carregados 4 atlas dedicados no startup (`16px` para o editor, `15px` para RayGUI/títulos, `13px` para HUD/Terminal/Blob/Status e `32px` com *mipmaps* + `TEXTURE_FILTER_TRILINEAR` para o canvas 2D com zoom).
- **Snap de coordenadas para pixels inteiros**: arredondamento em [`GuiDrawTextCentered`](include/raygui.h), HUD de observabilidade (corrigido incremento `.5px`), pílula de zoom, Blob, editor e barra de status para eliminar borrão sub-pixel.
- **Correção de texto vazando dos botões**: abas do Terminal Retrátil (`Diagnóstico & Compilador (GCC/ASan)`, `Saída do Programa (stdout / printf)`), botão `✕ Ocultar ▼` e `↺ Resetar` agora calculam a largura dinamicamente via `MeasureTextEx`, com `BeginScissorMode` de segurança nos botões da RayGUI.
- Adicionado glifo `⏸` (`0x23F8`) para o botão `Pausar Auto`.

### Ergonomia do Editor & Visibilidade do Blob
- **Rolamento suave com limites (*clamping*)**: interpolação fluida (`scroll_y` / `scroll_x` e terminal), suporte a scroll horizontal nativo (`GetMouseWheelMoveV`) + `Shift + Scroll` + `PageUp`/`PageDown`, impedindo scroll além do fim do arquivo ou através do terminal aberto.
- **Undo / Redo contínuo e acelerado**: segurar `Ctrl+Z` (ou `Ctrl+Y` / `Ctrl+Shift+Z`) agora repete rapidamente (~20 passos/s e acelera para ~40 passos/s ao segurar) com histórico expandido de 48 para **128 estados**.
- **Blob e pílula de câmera sempre visíveis**: ao abrir o Terminal Retrátil, o mascote Blob (junto com seu card interativo) e a pílula `[ Auto | - 100% + ]` acompanham suavemente a altura do painel e nunca ficam encobertos.

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
