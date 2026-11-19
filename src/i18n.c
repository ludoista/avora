#include "visualizer.h"

static AppLanguage g_current_lang = LANG_PT_BR;

void i18n_set_language(AppLanguage lang)
{
    if (lang >= 0 && lang < LANG_COUNT) {
        g_current_lang = lang;
    } else {
        g_current_lang = LANG_PT_BR;
    }
}

AppLanguage i18n_get_language(void)
{
    return g_current_lang;
}

static const char *g_strings[STR_COUNT][LANG_COUNT] = {
    /* STR_MENU_FILE */
    { "Arquivo", "File", "Archivo" },
    /* STR_MENU_EDIT */
    { "Editar", "Edit", "Editar" },
    /* STR_MENU_RUN */
    { "Executar", "Run", "Ejecutar" },
    /* STR_MENU_WINDOW */
    { "Janela", "Window", "Ventana" },
    /* STR_MENU_HELP */
    { "Ajuda", "Help", "Ayuda" },
    /* STR_BTN_SETTINGS */
    { "⚙ Configurações", "⚙ Settings", "⚙ Ajustes" },
    /* STR_BTN_RESET */
    { "↺ Resetar", "↺ Reset", "↺ Reiniciar" },
    /* STR_BTN_COMPILE_START */
    { "▶ Compilar & Iniciar", "▶ Compile & Start", "▶ Compilar e Iniciar" },
    /* STR_BTN_STOP */
    { "■ Parar", "■ Stop", "■ Detener" },
    /* STR_BTN_STEP */
    { "⏭ Passo", "⏭ Step", "⏭ Paso" },
    /* STR_BTN_STEP_BACK */
    { "⏮ Voltar", "⏮ Back", "⏮ Atrás" },
    /* STR_BTN_CONTINUE */
    { "▶ Continuar", "▶ Continue", "▶ Continuar" },
    /* STR_BTN_AUTOPLAY */
    { "⏯ Auto-Play", "⏯ Auto-Play", "⏯ Auto-Play" },
    /* STR_BTN_PAUSE_AUTO */
    { "⏸ Pausar Auto", "⏸ Pause Auto", "⏸ Pausar Auto" },
    /* STR_TAB_DIAGNOSTICS */
    { "Diagnóstico & Compilador (GCC/ASan)", "Diagnostics & Compiler (GCC/ASan)", "Diagnóstico y Compilador (GCC/ASan)" },
    /* STR_TAB_TERMINAL */
    { "Terminal", "Terminal", "Terminal" },
    /* STR_TAB_TERMINAL_NEW */
    { "● Terminal", "● Terminal", "● Terminal" },
    /* STR_BTN_HIDE_TERMINAL */
    { "✕ Ocultar ▼", "✕ Hide ▼", "✕ Ocultar ▼" },
    /* STR_HUD_HEADER_FMT */
    { "%s Pilha (%d) & Variáveis Locais (%d)", "%s Call Stack (%d) & Local Vars (%d)", "%s Pila (%d) y Variables Locales (%d)" },
    /* STR_HUD_CALL_STACK */
    { "PILHA DE CHAMADAS (CALL STACK)", "CALL STACK FRAMES", "PILA DE LLAMADAS (CALL STACK)" },
    /* STR_HUD_LOCAL_VARS */
    { "VARIÁVEIS DO FRAME ATUAL", "CURRENT FRAME VARIABLES", "VARIABLES DEL MARCO ACTUAL" },
    /* STR_HUD_NO_VARS */
    { "(sem variáveis locais no escopo)", "(no local variables in scope)", "(sin variables locales en el ámbito)" },
    /* STR_CANVAS_EMPTY_IDLE */
    { "Canvas de Memória — Clique em [ Compilar & Iniciar ] (ou F5) para depurar Árvores, Listas, Vetores ou Código C",
      "Memory Canvas — Click [ Compile & Start ] (or F5) to debug Trees, Linked Lists, Arrays or C Code",
      "Lienzo de Memoria — Haz clic en [ Compilar e Iniciar ] (o F5) para depurar Árboles, Listas, Vectores o Código C" },
    /* STR_CANVAS_EMPTY_RUN */
    { "Executando código sem alocações na Heap — Acompanhe a Pilha, Variáveis no HUD e o Terminal (stdout)",
      "Executing code without Heap allocations — Watch Call Stack, Local Vars in HUD and Terminal (stdout)",
      "Ejecutando código sin asignaciones en Heap — Sigue la Pila, Variables en el HUD y la Terminal (stdout)" },
    /* STR_CANVAS_STAGING_BOX */
    { "Alocação Temporária / Nós Locais (Heap)", "Temporary Staging / Local Heap Nodes", "Asignación Temporal / Nodos Locales (Heap)" },
    /* STR_SETTINGS_TITLE */
    { "⚙ Configurações & Preferências do AVORA", "⚙ AVORA Settings & Preferences", "⚙ Configuración y Preferencias de AVORA" },
    /* STR_SETTINGS_TAB_APPEARANCE */
    { "Aparência & Idioma", "Appearance & Language", "Apariencia e Idioma" },
    /* STR_SETTINGS_TAB_EDITOR */
    { "Editor de Código", "Code Editor", "Editor de Código" },
    /* STR_SETTINGS_TAB_DEBUGGER */
    { "Depurador (GCC/GDB)", "Debugger (GCC/GDB)", "Depurador (GCC/GDB)" },
    /* STR_SETTINGS_TAB_TEMPLATES */
    { "Templates & Janela", "Templates & Window", "Plantillas y Ventana" },
    /* STR_SETTINGS_THEME_HDR */
    { "Tema de Cores (Inspirado no Monkeytype)", "Color Theme (Monkeytype Inspired)", "Tema de Colores (Inspirado en Monkeytype)" },
    /* STR_SETTINGS_LANG_HDR */
    { "Idioma da Interface (i18n):", "Interface Language (i18n):", "Idioma de la Interfaz (i18n):" },
    /* STR_SETTINGS_BG_HDR */
    { "Estilo do Fundo do Canvas 2D", "2D Canvas Background Style", "Estilo de Fondo del Lienzo 2D" },
    /* STR_SETTINGS_RESTORE_DEFAULTS */
    { "↺ Restaurar Padrões", "↺ Restore Defaults", "↺ Restaurar Valores" },
    /* STR_SETTINGS_CLOSE */
    { "Fechar (Esc)", "Close (Esc)", "Cerrar (Esc)" },
    /* STR_FILE_NEW */
    { "Novo Scratchpad", "New Scratchpad", "Nuevo Scratchpad" },
    /* STR_FILE_OPEN */
    { "Abrir Arquivo .c...", "Open .c File...", "Abrir Archivo .c..." },
    /* STR_FILE_RECENT */
    { "Abrir Recente", "Open Recent", "Abrir Reciente" },
    /* STR_FILE_SAVE */
    { "Salvar", "Save", "Guardar" },
    /* STR_FILE_SAVE_AS */
    { "Salvar Como...", "Save As...", "Guardar Como..." },
    /* STR_FILE_SAVE_TPL */
    { "Salvar na Pasta de Templates...", "Save to Templates Folder...", "Guardar en Carpeta de Plantillas..." },
    /* STR_FILE_EXPORT_PNG */
    { "Exportar Canvas como PNG...", "Export Canvas as PNG...", "Exportar Lienzo como PNG..." },
    /* STR_FILE_OPEN_TPL_DIR */
    { "Abrir Pasta de Templates", "Open Templates Folder", "Abrir Carpeta de Plantillas" },
    /* STR_FILE_EXIT */
    { "Sair", "Exit", "Salir" },
    /* STR_EDIT_UNDO */
    { "Desfazer", "Undo", "Deshacer" },
    /* STR_EDIT_REDO */
    { "Refazer", "Redo", "Rehacer" },
    /* STR_EDIT_CUT */
    { "Recortar", "Cut", "Cortar" },
    /* STR_EDIT_COPY */
    { "Copiar", "Copy", "Copiar" },
    /* STR_EDIT_PASTE */
    { "Colar", "Paste", "Pegar" },
    /* STR_EDIT_SELECT_ALL */
    { "Selecionar Tudo", "Select All", "Seleccionar Todo" },
    /* STR_EDIT_RESET_TPL */
    { "Resetar Template Atual", "Reset Current Template", "Reiniciar Plantilla Actual" },
    /* STR_EDIT_PREFERENCES */
    { "Configurações / Preferências...", "Settings / Preferences...", "Configuración / Preferencias..." },
    /* STR_HELP_SHORTCUTS */
    { "Atalhos de Teclado...", "Keyboard Shortcuts...", "Atajos de Teclado..." },
    /* STR_HELP_BLOB_GUIDE */
    { "Abrir Guia Interativo do Blob", "Open Interactive Blob Guide", "Abrir Guía Interactiva de Blob" },
    /* STR_HELP_SYSDIAG */
    { "Diagnóstico do Sistema & Suporte...", "System Diagnostics & Support...", "Diagnóstico del Sistema y Soporte..." },
    /* STR_HELP_OPEN_CFG_DIR */
    { "Abrir Pasta de Configuração", "Open Configuration Folder", "Abrir Carpeta de Configuración" },
    /* STR_HELP_ABOUT */
    { "Sobre o AVORA...", "About AVORA...", "Acerca de AVORA..." },
    /* STR_SYSDIAG_TITLE */
    { "🩺 Diagnóstico do Sistema & Relatório de Suporte", "🩺 System Diagnostics & Support Report", "🩺 Diagnóstico del Sistema e Informe de Soporte" },
    /* STR_SYSDIAG_COPY_MD */
    { "Copiar Relatório de Ambiente (Markdown)", "Copy Environment Report (Markdown)", "Copiar Informe de Entorno (Markdown)" },
    /* STR_SYSDIAG_COPIED_TOAST */
    { "Relatório de ambiente em Markdown copiado para a área de transferência!",
      "Markdown environment report copied to clipboard!",
      "¡Informe de entorno en Markdown copiado al portapapeles!" },
    /* STR_STATUS_EDIT_FMT */
    { "● [MODO EDIÇÃO] F5 Compilar & Iniciar | Clique na margem p/ Breakpoint — Terminal %s",
      "● [EDIT MODE] F5 Compile & Start | Click gutter for Breakpoint — Terminal %s",
      "● [MODO EDICIÓN] F5 Compilar e Iniciar | Clic en margen p/ Breakpoint — Terminal %s" },
    /* STR_STATUS_DEBUG_FMT */
    { "● [DEPURANDO] Linha %d (%s) | %d nós na Heap | Passo %d/%d — Terminal %s",
      "● [DEBUGGING] Line %d (%s) | %d Heap nodes | Step %d/%d — Terminal %s",
      "● [DEPURANDO] Línea %d (%s) | %d nodos en Heap | Paso %d/%d — Terminal %s" },
    /* STR_STATUS_DONE_FMT */
    { "● [CONCLUÍDO] %d nós na memória Heap%s — Clique para detalhes %s",
      "● [FINISHED] %d nodes in Heap memory%s — Click for details %s",
      "● [COMPLETADO] %d nodos en memoria Heap%s — Clic para detalles %s" }
};

const char *tr(StrId id)
{
    if (id < 0 || id >= STR_COUNT) return "";
    int l = (g_current_lang >= 0 && g_current_lang < LANG_COUNT) ? (int)g_current_lang : 0;
    const char *s = g_strings[id][l];
    return s ? s : g_strings[id][0];
}
