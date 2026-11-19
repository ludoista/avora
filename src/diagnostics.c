#include "visualizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*
 * Sanitize UTF-8 strings from GCC/GDB locale output:
 * Replaces smart quotes (‘ ’ “ ”) and em-dashes with ASCII equivalents
 * so they never render as '?' in Raylib's Latin-1 glyph atlas.
 */
static void sanitize_utf8_internal(char *str, size_t max_len, bool preserve_newlines)
{
    if (!str) return;
    char out[4096];
    if (max_len > sizeof(out)) max_len = sizeof(out);
    size_t r = 0, w = 0;
    size_t len = strlen(str);

    while (r < len && w < max_len - 2) {
        unsigned char c0 = (unsigned char)str[r];
        unsigned char c1 = (r + 1 < len) ? (unsigned char)str[r + 1] : 0;
        unsigned char c2 = (r + 2 < len) ? (unsigned char)str[r + 2] : 0;

        /* UTF-8 Smart Single Quotes: ‘ (\xe2\x80\x98) and ’ (\xe2\x80\x99) */
        if (c0 == 0xE2 && c1 == 0x80 && (c2 == 0x98 || c2 == 0x99 || c2 == 0xB2)) {
            out[w++] = '\'';
            r += 3;
            continue;
        }
        /* UTF-8 Smart Double Quotes: “ (\xe2\x80\x9c) and ” (\xe2\x80\x9d) */
        if (c0 == 0xE2 && c1 == 0x80 && (c2 == 0x9C || c2 == 0x9D)) {
            out[w++] = '"';
            r += 3;
            continue;
        }
        /* UTF-8 En/Em-dash: – (\xe2\x80\x93) and — (\xe2\x80\x94) */
        if (c0 == 0xE2 && c1 == 0x80 && (c2 == 0x93 || c2 == 0x94)) {
            out[w++] = '-';
            r += 3;
            continue;
        }
        /* Strip 4-byte UTF-8 sequences (0xF0..0xF4) not in bitmap font */
        if (c0 >= 0xF0) {
            r += 4;
            continue;
        }

        if (c0 == '\r') {
            r++;
            continue;
        }
        if (c0 == '\t') {
            if (preserve_newlines && w + 4 < max_len - 2) {
                out[w++] = ' '; out[w++] = ' '; out[w++] = ' '; out[w++] = ' ';
            } else {
                out[w++] = ' ';
            }
            r++;
            continue;
        }
        if (c0 == '\n') {
            out[w++] = preserve_newlines ? '\n' : ' ';
            r++;
            continue;
        }

        out[w++] = (char)c0;
        r++;
    }
    out[w] = '\0';
    memcpy(str, out, w + 1);
}

void sanitize_utf8_text(char *str)
{
    sanitize_utf8_internal(str, 2048, false);
}

void diagnostics_clear(DiagnosticReport *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->error_type = ERR_NONE;
    report->fault_line = 0;
    snprintf(
        report->pedagogical_title,
        sizeof(report->pedagogical_title),
        "AVORA Pronto — Laboratório Visual de Algoritmos e Estruturas de Dados"
    );
    snprintf(
        report->pedagogical_explanation,
        sizeof(report->pedagogical_explanation),
        "Escolha um template (Buscas, BST, AVL, Rubro-Negra, Árvore B, Listas, Vetores ou LeetCode) ou escreva seu código C. "
        "Clique em [Compilar & Iniciar] para inspecionar ponteiros, variáveis locais e pilha de chamadas!"
    );
}

void diagnostics_set_compile_error(DiagnosticReport *report, const char *raw_gcc_err)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->error_type = ERR_COMPILE;
    report->fault_line = 1;

    if (raw_gcc_err) {
        snprintf(report->raw_compiler_log, sizeof(report->raw_compiler_log), "%s", raw_gcc_err);
        sanitize_utf8_internal(report->raw_compiler_log, sizeof(report->raw_compiler_log), true);

        snprintf(report->raw_trace, sizeof(report->raw_trace), "%s", raw_gcc_err);
        sanitize_utf8_text(report->raw_trace);
    }

    /* Extract line number from "student_code.c:<line>:" (skipping "student_code.c: In function...") */
    const char *search = report->raw_trace;
    const char *msg_start = report->raw_trace;
    while ((search = strstr(search, "student_code.c:")) != NULL) {
        const char *p = search + strlen("student_code.c:");
        if (*p >= '0' && *p <= '9') {
            int parsed_line = atoi(p);
            if (parsed_line > 0) {
                report->fault_line = parsed_line;
            }
            const char *err_tok = strstr(p, "error:");
            if (err_tok) {
                msg_start = err_tok + strlen("error:");
                while (*msg_start == ' ') msg_start++;
            }
            break;
        }
        search = p;
    }

    snprintf(
        report->pedagogical_title,
        sizeof(report->pedagogical_title),
        "[ERRO DE COMPILAÇÃO] O GCC encontrou um problema na Linha %d",
        report->fault_line
    );

    if (strstr(report->raw_trace, "expected ';'") != NULL ||
        strstr(report->raw_trace, "expected ',' or ';'") != NULL) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Parece que faltou um ponto e vírgula ';' logo antes da linha %d. "
            "Dê uma olhada no final da linha anterior! Mensagem do GCC: %.240s",
            report->fault_line,
            msg_start
        );
    } else if (strstr(report->raw_trace, " is not a pointer") ||
               strstr(report->raw_trace, "invalid type argument of '->'")) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Na linha %d, você usou '->' em algo que não é um ponteiro (ou usou '.' em um struct TreeNode*). "
            "Lembre-se: ponteiros para structs sempre usam '->' (ex: no->val). Mensagem do GCC: %.240s",
            report->fault_line,
            msg_start
        );
    } else if (strstr(report->raw_trace, "undeclared") != NULL ||
               strstr(report->raw_trace, "implicit declaration") != NULL) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Na linha %d há um nome de variável ou função que ainda não foi declarado. "
            "Confira se não houve erro de digitação ou se a função foi definida antes de ser chamada! GCC: %.240s",
            report->fault_line,
            msg_start
        );
    } else if (strstr(report->raw_trace, "has no member named") != NULL) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "A struct TreeNode tem os campos 'val', 'left' e 'right' (e ListNode tem 'val', 'next', 'prev'). "
            "Na linha %d você tentou acessar um campo com outro nome. GCC: %.240s",
            report->fault_line,
            msg_start
        );
    } else {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Não foi possível compilar devido a um erro de sintaxe ou tipos na linha %d: %.320s",
            report->fault_line,
            msg_start
        );
    }

    sanitize_utf8_text(report->pedagogical_explanation);
}

void diagnostics_set_runtime_trap(
    DiagnosticReport *report,
    ErrorType         type,
    int               fault_line,
    uint64_t          fault_addr,
    uint64_t          secondary_addr,
    const char       *raw_trace
)
{
    if (!report) return;
    report->error_type     = type;
    report->fault_line     = fault_line;
    report->fault_addr     = fault_addr;
    report->secondary_addr = secondary_addr;

    if (raw_trace) {
        snprintf(report->raw_compiler_log, sizeof(report->raw_compiler_log), "%s", raw_trace);
        sanitize_utf8_internal(report->raw_compiler_log, sizeof(report->raw_compiler_log), true);

        snprintf(report->raw_trace, sizeof(report->raw_trace), "%s", raw_trace);
        sanitize_utf8_text(report->raw_trace);
    } else {
        report->raw_trace[0] = '\0';
        report->raw_compiler_log[0] = '\0';
    }

    switch (type) {
        case ERR_SIGSEGV_NULL:
            snprintf(
                report->pedagogical_title,
                sizeof(report->pedagogical_title),
                "[SEGFAULT] Acesso a Ponteiro NULL na Linha %d",
                fault_line
            );
            snprintf(
                report->pedagogical_explanation,
                sizeof(report->pedagogical_explanation),
                "O código tentou ler ou escrever em um campo (->val, ->left, ->right ou ->next) quando o ponteiro valia NULL (0x0). "
                "Como proteger: sempre teste 'if (no == NULL) return ...;' como primeira linha da função recursiva ou antes de encadear ponteiros!"
            );
            break;

        case ERR_SIGSEGV_GARBAGE:
            snprintf(
                report->pedagogical_title,
                sizeof(report->pedagogical_title),
                "[LIXO DE MEMÓRIA] Ponteiro Não Inicializado na Linha %d",
                fault_line
            );
            snprintf(
                report->pedagogical_explanation,
                sizeof(report->pedagogical_explanation),
                "Um ponteiro contém o endereço inválido 0x%" PRIx64 ". "
                "Por que isso acontece? Em C, o malloc() apenas reserva bytes na Heap sem limpá-los! "
                "Sempre inicialize 'novo->left = NULL;' e 'novo->right = NULL;' (ou 'novo->next = NULL;') logo após alocar um nó.",
                fault_addr
            );
            break;

        case ERR_CYCLE_DETECTED:
            snprintf(
                report->pedagogical_title,
                sizeof(report->pedagogical_title),
                "[CICLO DETECTADO] Referência Circular na Estrutura (Linha %d)",
                fault_line
            );
            snprintf(
                report->pedagogical_explanation,
                sizeof(report->pedagogical_explanation),
                "O nó 0x%" PRIx64 " passou a apontar de volta para seu próprio ancestral (0x%" PRIx64 "), formando um loop fechado! "
                "Verifique a ordem das atribuições de ponteiros na sua rotação, inversão ou inserção.",
                fault_addr,
                secondary_addr
            );
            break;

        case ERR_ASAN_UAF:
            snprintf(
                report->pedagogical_title,
                sizeof(report->pedagogical_title),
                "[DANGLING POINTER] Uso de Memória Após free() na Linha %d",
                fault_line
            );
            snprintf(
                report->pedagogical_explanation,
                sizeof(report->pedagogical_explanation),
                "O endereço 0x%" PRIx64 " já foi devolvido ao sistema com free(), mas ainda está sendo referenciado! "
                "Dica: salve o ponteiro do próximo nó/filho em uma variável auxiliar antes de chamar free(no).",
                fault_addr
            );
            break;

        case ERR_ASAN_OOB:
            snprintf(
                report->pedagogical_title,
                sizeof(report->pedagogical_title),
                "[ESTOURO DE MEMÓRIA] Acesso Fora dos Limites na Linha %d",
                fault_line
            );
            snprintf(
                report->pedagogical_explanation,
                sizeof(report->pedagogical_explanation),
                "Foi detectada uma leitura ou escrita fora da região alocada (0x%" PRIx64 "). "
                "Verifique os índices do seu vetor (0 até N-1) e o tamanho passado ao malloc().",
                fault_addr
            );
            break;

        case ERR_INFINITE_LOOP:
            snprintf(
                report->pedagogical_title,
                sizeof(report->pedagogical_title),
                "[TEMPO LIMITE] Loop ou Recursão Infinita (> 1.5s na Linha %d)",
                fault_line
            );
            snprintf(
                report->pedagogical_explanation,
                sizeof(report->pedagogical_explanation),
                "A operação demorou mais de 1.5s sem avançar e foi interrompida com segurança. "
                "Confira se a condição de parada do seu while()/for() ou o caso base da recursão é alcançável!"
            );
            break;

        default:
            break;
    }
}

void diagnostics_inspect_snapshot(
    const TreeSnapshot *snapshot,
    DiagnosticReport   *report,
    DebuggerState      *state_inout
)
{
    if (!snapshot || !report) return;

    if (report->error_type == ERR_COMPILE ||
        report->error_type == ERR_SIGSEGV_NULL ||
        report->error_type == ERR_INFINITE_LOOP) {
        return;
    }

    static int cycle_consecutive_steps    = 0;
    static int dangling_consecutive_steps = 0;
    bool found_cycle_this_step      = false;
    bool found_dangling_this_step   = false;
    bool found_uninit_node_ptr      = false;
    uint64_t cycle_node_addr        = 0;
    uint64_t cycle_target_addr      = 0;
    uint64_t dangling_bad_addr      = 0;
    uint64_t dangling_parent_addr   = 0;
    uint64_t uninit_bad_addr        = 0;
    uint64_t uninit_parent_addr     = 0;

    for (int i = 0; i < snapshot->node_count; i++) {
        const VisualNode *n = &snapshot->nodes[i];

        if (n->is_dangling || n->left_state == EDGE_DANGLING || n->right_state == EDGE_DANGLING) {
            found_dangling_this_step = true;
            dangling_bad_addr = n->is_dangling ? n->addr :
                                (n->left_state == EDGE_DANGLING ? n->left_addr : n->right_addr);
            dangling_parent_addr = n->addr;
        }

        if (n->left_state == EDGE_CYCLE) {
            found_cycle_this_step = true;
            cycle_node_addr = n->addr;
            cycle_target_addr = n->left_addr;
        }

        if (n->right_state == EDGE_CYCLE) {
            found_cycle_this_step = true;
            cycle_node_addr = n->addr;
            cycle_target_addr = n->right_addr;
        }

        if (n->left_state == EDGE_GARBAGE_PTR || n->right_state == EDGE_GARBAGE_PTR) {
            found_uninit_node_ptr = true;
            uninit_bad_addr    = (n->left_state == EDGE_GARBAGE_PTR) ? n->left_addr : n->right_addr;
            uninit_parent_addr = n->addr;
        }
    }

    if (found_dangling_this_step) {
        dangling_consecutive_steps++;
        if (dangling_consecutive_steps >= 3) {
            diagnostics_set_runtime_trap(
                report,
                ERR_ASAN_UAF,
                snapshot->current_executing_line,
                dangling_bad_addr,
                dangling_parent_addr,
                "Detectado ponteiro pendente (dangling pointer) apontando para regiao ja liberada com free()."
            );
            return;
        }
    } else {
        dangling_consecutive_steps = 0;
    }

    if (found_cycle_this_step) {
        cycle_consecutive_steps++;
        if (cycle_consecutive_steps >= 2) {
            diagnostics_set_runtime_trap(
                report,
                ERR_CYCLE_DETECTED,
                snapshot->current_executing_line,
                cycle_node_addr,
                cycle_target_addr,
                "Ciclo estrutural persistente detectado pelo serializador BFS."
            );
            return;
        }
    } else {
        cycle_consecutive_steps = 0;
    }

    /* Escalate to Critical Orange Garbage Error ONLY if execution finished with uninitialized pointers */
    if (state_inout && *state_inout == STATE_FINISHED && found_uninit_node_ptr) {
        diagnostics_set_runtime_trap(
            report,
            ERR_SIGSEGV_GARBAGE,
            snapshot->current_executing_line,
            uninit_bad_addr,
            uninit_parent_addr,
            "Execucao finalizada com ponteiros ainda nao inicializados (lixo de memoria) na estrutura."
        );
        return;
    }

    report->error_type             = ERR_NONE;
    report->has_uninit_ptr_warning = found_uninit_node_ptr;
    report->fault_line             = snapshot->current_executing_line;

    if (found_uninit_node_ptr) {
        snprintf(
            report->pedagogical_title,
            sizeof(report->pedagogical_title),
            "Em Construção — Linha %d (%s) | Nós Ativos: %d | Ponteiros Aguardando Atribuição",
            snapshot->current_executing_line,
            snapshot->current_func[0] ? snapshot->current_func : "main",
            snapshot->node_count
        );
    } else if (report->auto_main_warning) {
        snprintf(
            report->pedagogical_title,
            sizeof(report->pedagogical_title),
            "Depurando — Linha %d (%s) | Nós na Heap: %d [main() Automático]",
            snapshot->current_executing_line,
            snapshot->current_func[0] ? snapshot->current_func : "main",
            snapshot->node_count
        );
    } else {
        snprintf(
            report->pedagogical_title,
            sizeof(report->pedagogical_title),
            "Depurando — Linha %d (%s) | Nós na Heap: %d | Vetores: %d | Pilha: %d frames",
            snapshot->current_executing_line,
            snapshot->current_func[0] ? snapshot->current_func : "main",
            snapshot->node_count,
            snapshot->array_count,
            snapshot->frame_count > 0 ? snapshot->frame_count : 1
        );
    }

    char vars_summary[384];
    vars_summary[0] = '\0';
    size_t offset = 0;
    for (int i = 0; i < snapshot->local_var_count; i++) {
        const LocalPointerVar *v = &snapshot->local_vars[i];
        int written = 0;
        if (v->is_null) {
            written = snprintf(vars_summary + offset, sizeof(vars_summary) - offset,
                               "%s=NULL  ", v->name);
        } else if (v->is_garbage) {
            written = snprintf(vars_summary + offset, sizeof(vars_summary) - offset,
                               "%s=[? não inic.]  ", v->name);
        } else {
            written = snprintf(vars_summary + offset, sizeof(vars_summary) - offset,
                               "%s=0x%" PRIx64 "  ", v->name, v->addr);
        }
        if (written > 0 && offset + (size_t)written < sizeof(vars_summary)) {
            offset += (size_t)written;
        }
    }

    if (found_uninit_node_ptr) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Nota: Nó recém-alocado com ponteiros ainda não inicializados (aguardando atribuição ou NULL).%s%s",
            offset > 0 ? "  |  Ponteiros locais: " : "",
            offset > 0 ? vars_summary : ""
        );
    } else if (report->auto_main_warning) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Dica do Blob: Você colou apenas a função %s() sem um main(), então criei um main() automático "
            "para você já sair testando! Ponteiros ativos: %s",
            report->auto_main_func[0] ? report->auto_main_func : "de solução",
            vars_summary[0] ? vars_summary : "nenhum"
        );
    } else if (offset > 0) {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Ponteiros locais neste frame da pilha: %s",
            vars_summary
        );
    } else {
        snprintf(
            report->pedagogical_explanation,
            sizeof(report->pedagogical_explanation),
            "Execução íntegra na memória. Avance com [Passo] ou [Auto-Play] e acompanhe o HUD de Variáveis & Call Stack!"
        );
    }
}
