#include "visualizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

/* ============================================================================
 * 1. COMPREHENSIVE 16-TEMPLATE ALGORITHMS, DATA STRUCTURES & LEETCODE LIBRARY
 * ============================================================================ */

/* 0. Vazio (Scratchpad Livre) */
static const char *PRESET_0_SCRATCHPAD =
    "// 0. Scratchpad Livre — Escreva seu algoritmo ou solução do LeetCode!\n"
    "// A macro BUILD_TREE(\"[...]\") monta qualquer árvore binária em nível (BFS).\n"
    "\n"
    "struct TreeNode* minhaSolucao(struct TreeNode* root) {\n"
    "    if (root == NULL) return NULL;\n"
    "    // Escreva sua lógica aqui:\n"
    "    return root;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* root = BUILD_TREE(\"[10, 5, 15, 2, 7, null, 20]\");\n"
    "    root = minhaSolucao(root);\n"
    "    return 0;\n"
    "}\n";

/* 1. LeetCode 538: Convert BST to Greater Tree (BUILD_TREE) */
static const char *PRESET_1_LEETCODE_STD =
    "// LeetCode 538. Convert BST to Greater Tree (Soma Acumulada Reversa)\n"
    "// Cole qualquer função do LeetCode e teste no main() com BUILD_TREE!\n"
    "\n"
    "void updateBST(struct TreeNode* root, int* sum) {\n"
    "    if (root == NULL) return;\n"
    "    updateBST(root->right, sum);\n"
    "    *sum += root->val;\n"
    "    root->val = *sum;\n"
    "    updateBST(root->left, sum);\n"
    "}\n"
    "\n"
    "struct TreeNode* convertBST(struct TreeNode* root) {\n"
    "    int sum = 0;\n"
    "    updateBST(root, &sum);\n"
    "    return root;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* root = BUILD_TREE(\"[4,1,6,0,2,5,7,null,null,null,3,null,null,null,8]\");\n"
    "    root = convertBST(root);\n"
    "    return 0;\n"
    "}\n";

/* 2. LeetCode 108: Convert Sorted Array to BST (main com Vetor Ordenado) */
static const char *PRESET_2_LEETCODE_ARRAY_BST =
    "// LeetCode 108. Convert Sorted Array to Binary Search Tree (main com Vetor)\n"
    "// Constrói uma BST perfeitamente balanceada dividindo o vetor ao meio recursivamente!\n"
    "// Repare como nós recém-alocados com malloc() ficam '[ ? não inic. ]' até receberem left/right.\n"
    "\n"
    "struct TreeNode* construirDeVetor(int* nums, int esq, int dir) {\n"
    "    if (esq > dir) return NULL;\n"
    "    int meio = esq + (dir - esq) / 2;\n"
    "    struct TreeNode* raiz = (struct TreeNode*)malloc(sizeof(struct TreeNode));\n"
    "    raiz->val = nums[meio];\n"
    "    raiz->left = construirDeVetor(nums, esq, meio - 1);\n"
    "    raiz->right = construirDeVetor(nums, meio + 1, dir);\n"
    "    return raiz;\n"
    "}\n"
    "\n"
    "struct TreeNode* sortedArrayToBST(int* nums, int numsSize) {\n"
    "    return construirDeVetor(nums, 0, numsSize - 1);\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    int nums[] = { -10, -3, 0, 5, 9, 12, 18 };\n"
    "    int tam = (int)(sizeof(nums) / sizeof(nums[0]));\n"
    "    struct TreeNode* raiz = sortedArrayToBST(nums, tam);\n"
    "    (void)raiz;\n"
    "    return 0;\n"
    "}\n";

/* 3. LeetCode 105: Construct Binary Tree from Preorder & Inorder Arrays (main com 2 Vetores) */
static const char *PRESET_3_LEETCODE_PRE_IN_ARRAY =
    "// LeetCode 105. Construct Binary Tree from Preorder and Inorder Traversal (main com 2 Vetores)\n"
    "// Reconstrói a árvore original usando o vetor Pré-Ordem para achar a raiz e Em-Ordem para dividir!\n"
    "\n"
    "struct TreeNode* montarPreIn(int* pre, int* preIdx, int* in, int inEsq, int inDir) {\n"
    "    if (inEsq > inDir) return NULL;\n"
    "    int valorRaiz = pre[(*preIdx)++];\n"
    "    struct TreeNode* raiz = new_node(valorRaiz);\n"
    "\n"
    "    int posMeio = inEsq;\n"
    "    while (posMeio <= inDir && in[posMeio] != valorRaiz) {\n"
    "        posMeio++;\n"
    "    }\n"
    "    raiz->left  = montarPreIn(pre, preIdx, in, inEsq, posMeio - 1);\n"
    "    raiz->right = montarPreIn(pre, preIdx, in, posMeio + 1, inDir);\n"
    "    return raiz;\n"
    "}\n"
    "\n"
    "struct TreeNode* buildTree(int* preorder, int preorderSize, int* inorder, int inorderSize) {\n"
    "    (void)preorderSize;\n"
    "    int preIdx = 0;\n"
    "    return montarPreIn(preorder, &preIdx, inorder, 0, inorderSize - 1);\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    int preorder[] = { 3, 9, 20, 15, 7 };\n"
    "    int inorder[]  = { 9, 3, 15, 20, 7 };\n"
    "    int n = 5;\n"
    "    struct TreeNode* raiz = buildTree(preorder, n, inorder, n);\n"
    "    (void)raiz;\n"
    "    return 0;\n"
    "}\n";

/* 4. LeetCode 617: Merge Two Binary Trees (main com Floresta de 2 Árvores) */
static const char *PRESET_4_LEETCODE_MERGE_FOREST =
    "// LeetCode 617. Merge Two Binary Trees (main com Floresta: 2 Árvores Lado a Lado)\n"
    "// Funde arvore1 e arvore2 somando os nós sobrepostos e costurando subárvores!\n"
    "\n"
    "struct TreeNode* mergeTrees(struct TreeNode* t1, struct TreeNode* t2) {\n"
    "    if (t1 == NULL) return t2;\n"
    "    if (t2 == NULL) return t1;\n"
    "    t1->val += t2->val;\n"
    "    t1->left  = mergeTrees(t1->left, t2->left);\n"
    "    t1->right = mergeTrees(t1->right, t2->right);\n"
    "    return t1;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* arvore1 = BUILD_TREE(\"[1, 3, 2, 5]\");\n"
    "    struct TreeNode* arvore2 = BUILD_TREE(\"[2, 1, 3, null, 4, null, 7]\");\n"
    "    arvore1 = mergeTrees(arvore1, arvore2);\n"
    "    return 0;\n"
    "}\n";

/* 5. LeetCode 100: Same Tree (main com Floresta de Comparação) */
static const char *PRESET_5_SAME_TREE_FOREST =
    "// LeetCode 100. Same Tree — Comparação de Múltiplas Árvores Lado a Lado (Floresta)!\n"
    "\n"
    "bool isSameTree(struct TreeNode* p, struct TreeNode* q) {\n"
    "    if (p == NULL && q == NULL) return true;\n"
    "    if (p == NULL || q == NULL) return false;\n"
    "    if (p->val != q->val) return false;\n"
    "    return isSameTree(p->left, q->left) && isSameTree(p->right, q->right);\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* arvore1 = BUILD_TREE(\"[10, 5, 20, 3, 7, null, 30]\");\n"
    "    struct TreeNode* arvore2 = BUILD_TREE(\"[10, 5, 20, 3, 99, null, 30]\");\n"
    "    bool iguais = isSameTree(arvore1, arvore2);\n"
    "    (void)iguais;\n"
    "    return 0;\n"
    "}\n";

/* 6. LeetCode 226 & 572: Invert Binary Tree + Subtree of Another Tree (Floresta) */
static const char *PRESET_6_INVERT_AND_SUBTREE =
    "// LeetCode 226 (Invert Tree) + 572 (Subtree of Another Tree) em Floresta\n"
    "// Primeiro espelha a subárvore candidata e depois verifica se ela encaixa na árvore principal!\n"
    "\n"
    "struct TreeNode* invertTree(struct TreeNode* root) {\n"
    "    if (root == NULL) return NULL;\n"
    "    struct TreeNode* temp = root->left;\n"
    "    root->left  = invertTree(root->right);\n"
    "    root->right = invertTree(temp);\n"
    "    return root;\n"
    "}\n"
    "\n"
    "bool saoIdenticas(struct TreeNode* a, struct TreeNode* b) {\n"
    "    if (a == NULL && b == NULL) return true;\n"
    "    if (a == NULL || b == NULL) return false;\n"
    "    return (a->val == b->val) && saoIdenticas(a->left, b->left) && saoIdenticas(a->right, b->right);\n"
    "}\n"
    "\n"
    "bool isSubtree(struct TreeNode* root, struct TreeNode* subRoot) {\n"
    "    if (root == NULL) return false;\n"
    "    if (saoIdenticas(root, subRoot)) return true;\n"
    "    return isSubtree(root->left, subRoot) || isSubtree(root->right, subRoot);\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* principal = BUILD_TREE(\"[3, 4, 5, 1, 2]\");\n"
    "    struct TreeNode* candidata = BUILD_TREE(\"[4, 2, 1]\");\n"
    "    candidata = invertTree(candidata); // Desvira [4, 2, 1] para [4, 1, 2]\n"
    "    bool achouSub = isSubtree(principal, candidata);\n"
    "    (void)achouSub;\n"
    "    return 0;\n"
    "}\n";

/* 2. Buscas: Busca Binária (BST), Busca em Profundidade (DFS) e Largura (BFS) */
static const char *PRESET_2_SEARCHES =
    "// Comparação de Buscas: Busca Binária O(log n), DFS (Profundidade) e BFS (Largura)\n"
    "// Observe como os ponteiros 'atual', 'dfs' e 'bfs' percorrem a árvore!\n"
    "\n"
    "struct TreeNode* buscaBinariaBST(struct TreeNode* atual, int alvo) {\n"
    "    while (atual != NULL && atual->val != alvo) {\n"
    "        if (alvo < atual->val)\n"
    "            atual = atual->left;\n"
    "        else\n"
    "            atual = atual->right;\n"
    "    }\n"
    "    return atual;\n"
    "}\n"
    "\n"
    "struct TreeNode* buscaProfundidadeDFS(struct TreeNode* dfs, int alvo) {\n"
    "    if (dfs == NULL || dfs->val == alvo) return dfs;\n"
    "    struct TreeNode* esq = buscaProfundidadeDFS(dfs->left, alvo);\n"
    "    if (esq != NULL) return esq;\n"
    "    return buscaProfundidadeDFS(dfs->right, alvo);\n"
    "}\n"
    "\n"
    "struct TreeNode* buscaLarguraBFS(struct TreeNode* raiz, int alvo) {\n"
    "    if (raiz == NULL) return NULL;\n"
    "    struct TreeNode* fila[32];\n"
    "    int ini = 0, fim = 0;\n"
    "    fila[fim++] = raiz;\n"
    "    while (ini < fim) {\n"
    "        struct TreeNode* bfs = fila[ini++];\n"
    "        if (bfs->val == alvo) return bfs;\n"
    "        if (bfs->left != NULL)  fila[fim++] = bfs->left;\n"
    "        if (bfs->right != NULL) fila[fim++] = bfs->right;\n"
    "    }\n"
    "    return NULL;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = BUILD_TREE(\"[50, 30, 70, 20, 40, 60, 80]\");\n"
    "    struct TreeNode* r1 = buscaBinariaBST(raiz, 40);\n"
    "    struct TreeNode* r2 = buscaLarguraBFS(raiz, 60);\n"
    "    struct TreeNode* r3 = buscaProfundidadeDFS(raiz, 80);\n"
    "    (void)r1; (void)r2; (void)r3;\n"
    "    return 0;\n"
    "}\n";

/* 3. Árvore Binária de Busca (BST): Inserção & Remoção (3 Casos) */
static const char *PRESET_3_BST_INSERT_REMOVE =
    "// BST: Inserção e Remoção Clássica (Nó Folha, 1 Filho e 2 Filhos com Sucessor)\n"
    "\n"
    "struct TreeNode* menorNo(struct TreeNode* atual) {\n"
    "    while (atual && atual->left != NULL)\n"
    "        atual = atual->left;\n"
    "    return atual;\n"
    "}\n"
    "\n"
    "struct TreeNode* removerBST(struct TreeNode* raiz, int chave) {\n"
    "    if (raiz == NULL) return NULL;\n"
    "    if (chave < raiz->val) {\n"
    "        raiz->left = removerBST(raiz->left, chave);\n"
    "    } else if (chave > raiz->val) {\n"
    "        raiz->right = removerBST(raiz->right, chave);\n"
    "    } else {\n"
    "        if (raiz->left == NULL) {\n"
    "            struct TreeNode* temp = raiz->right;\n"
    "            free(raiz);\n"
    "            return temp;\n"
    "        } else if (raiz->right == NULL) {\n"
    "            struct TreeNode* temp = raiz->left;\n"
    "            free(raiz);\n"
    "            return temp;\n"
    "        }\n"
    "        struct TreeNode* sucessor = menorNo(raiz->right);\n"
    "        raiz->val = sucessor->val;\n"
    "        raiz->right = removerBST(raiz->right, sucessor->val);\n"
    "    }\n"
    "    return raiz;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = BUILD_TREE(\"[50, 30, 70, 20, 40, 60, 80]\");\n"
    "    raiz = removerBST(raiz, 20); // Caso 1: Remove folha (20)\n"
    "    raiz = removerBST(raiz, 30); // Caso 2: Remove nó com 1 filho (30)\n"
    "    raiz = removerBST(raiz, 50); // Caso 3: Remove raiz com 2 filhos (50)\n"
    "    return 0;\n"
    "}\n";

/* 4. Travessias em Árvores (Pré-Ordem, Em-Ordem e Pós-Ordem) */
static const char *PRESET_4_TRAVERSALS =
    "// Travessias Recursivas: Pré-Ordem (Raiz-E-D), Em-Ordem (E-Raiz-D) e Pós-Ordem (E-D-Raiz)\n"
    "\n"
    "void preOrdem(struct TreeNode* atual, int* seq, int* k) {\n"
    "    if (atual == NULL) return;\n"
    "    seq[(*k)++] = atual->val;\n"
    "    preOrdem(atual->left, seq, k);\n"
    "    preOrdem(atual->right, seq, k);\n"
    "}\n"
    "\n"
    "void emOrdem(struct TreeNode* atual, int* seq, int* k) {\n"
    "    if (atual == NULL) return;\n"
    "    emOrdem(atual->left, seq, k);\n"
    "    seq[(*k)++] = atual->val;\n"
    "    emOrdem(atual->right, seq, k);\n"
    "}\n"
    "\n"
    "void posOrdem(struct TreeNode* atual, int* seq, int* k) {\n"
    "    if (atual == NULL) return;\n"
    "    posOrdem(atual->left, seq, k);\n"
    "    posOrdem(atual->right, seq, k);\n"
    "    seq[(*k)++] = atual->val;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = BUILD_TREE(\"[40, 20, 60, 10, 30, 50, 70]\");\n"
    "    int visitados[32];\n"
    "    int k = 0;\n"
    "    preOrdem(raiz, visitados, &k);\n"
    "    k = 0;\n"
    "    emOrdem(raiz, visitados, &k);\n"
    "    k = 0;\n"
    "    posOrdem(raiz, visitados, &k);\n"
    "    return 0;\n"
    "}\n";

/* 5. Árvore AVL (Balanceamento Automático + Rotações LL, RR, LR, RL) */
static const char *PRESET_5_AVL =
    "// Árvore AVL: Fator de Balanceamento e Rotações Simples/Duplas (LL, RR, LR, RL)\n"
    "\n"
    "int altura(struct TreeNode* n) {\n"
    "    if (n == NULL) return 0;\n"
    "    int ae = altura(n->left);\n"
    "    int ad = altura(n->right);\n"
    "    return 1 + (ae > ad ? ae : ad);\n"
    "}\n"
    "\n"
    "struct TreeNode* rotacaoDireita(struct TreeNode* y) {\n"
    "    struct TreeNode* x = y->left;\n"
    "    struct TreeNode* T2 = x->right;\n"
    "    x->right = y;\n"
    "    y->left = T2;\n"
    "    return x;\n"
    "}\n"
    "\n"
    "struct TreeNode* rotacaoEsquerda(struct TreeNode* x) {\n"
    "    struct TreeNode* y = x->right;\n"
    "    struct TreeNode* T2 = y->left;\n"
    "    y->left = x;\n"
    "    x->right = T2;\n"
    "    return y;\n"
    "}\n"
    "\n"
    "struct TreeNode* inserirAVL(struct TreeNode* no, int val) {\n"
    "    if (no == NULL) return new_node(val);\n"
    "    if (val < no->val)\n"
    "        no->left = inserirAVL(no->left, val);\n"
    "    else if (val > no->val)\n"
    "        no->right = inserirAVL(no->right, val);\n"
    "    else\n"
    "        return no;\n"
    "\n"
    "    int fb = altura(no->left) - altura(no->right);\n"
    "    if (fb > 1 && val < no->left->val)\n"
    "        return rotacaoDireita(no);\n"
    "    if (fb < -1 && val > no->right->val)\n"
    "        return rotacaoEsquerda(no);\n"
    "    if (fb > 1 && val > no->left->val) {\n"
    "        no->left = rotacaoEsquerda(no->left);\n"
    "        return rotacaoDireita(no);\n"
    "    }\n"
    "    if (fb < -1 && val < no->right->val) {\n"
    "        no->right = rotacaoDireita(no->right);\n"
    "        return rotacaoEsquerda(no);\n"
    "    }\n"
    "    return no;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = NULL;\n"
    "    int seq[] = { 10, 20, 30, 40, 50, 25 };\n"
    "    for (int i = 0; i < 6; i++) {\n"
    "        raiz = inserirAVL(raiz, seq[i]);\n"
    "    }\n"
    "    return 0;\n"
    "}\n";

/* 6. Árvore Rubro-Negra (Left-Leaning Red-Black Tree — LLRB) */
static const char *PRESET_6_RED_BLACK =
    "// Árvore Rubro-Negra (Left-Leaning Red-Black Tree de Sedgewick)\n"
    "// Mantém balanceamento perfeito usando Rotações e Inversão de Cores (Flip)!\n"
    "\n"
    "struct TreeNode* nosVermelhos[64];\n"
    "int qtdVermelhos = 0;\n"
    "\n"
    "bool ehVermelho(struct TreeNode* no) {\n"
    "    if (no == NULL) return false;\n"
    "    for (int i = 0; i < qtdVermelhos; i++) {\n"
    "        if (nosVermelhos[i] == no) return true;\n"
    "    }\n"
    "    return false;\n"
    "}\n"
    "\n"
    "void definirCor(struct TreeNode* no, bool vermelho) {\n"
    "    if (no == NULL) return;\n"
    "    for (int i = 0; i < qtdVermelhos; i++) {\n"
    "        if (nosVermelhos[i] == no) {\n"
    "            if (!vermelho) nosVermelhos[i] = nosVermelhos[--qtdVermelhos];\n"
    "            return;\n"
    "        }\n"
    "    }\n"
    "    if (vermelho && qtdVermelhos < 64) nosVermelhos[qtdVermelhos++] = no;\n"
    "}\n"
    "\n"
    "struct TreeNode* rotEsqRB(struct TreeNode* h) {\n"
    "    struct TreeNode* x = h->right;\n"
    "    h->right = x->left;\n"
    "    x->left = h;\n"
    "    definirCor(x, ehVermelho(h));\n"
    "    definirCor(h, true);\n"
    "    return x;\n"
    "}\n"
    "\n"
    "struct TreeNode* rotDirRB(struct TreeNode* h) {\n"
    "    struct TreeNode* x = h->left;\n"
    "    h->left = x->right;\n"
    "    x->right = h;\n"
    "    definirCor(x, ehVermelho(h));\n"
    "    definirCor(h, true);\n"
    "    return x;\n"
    "}\n"
    "\n"
    "void inverterCores(struct TreeNode* h) {\n"
    "    definirCor(h, true);\n"
    "    definirCor(h->left, false);\n"
    "    definirCor(h->right, false);\n"
    "}\n"
    "\n"
    "struct TreeNode* inserirRB(struct TreeNode* h, int val) {\n"
    "    if (h == NULL) {\n"
    "        struct TreeNode* novo = new_node(val);\n"
    "        definirCor(novo, true);\n"
    "        return novo;\n"
    "    }\n"
    "    if (val < h->val)      h->left  = inserirRB(h->left, val);\n"
    "    else if (val > h->val) h->right = inserirRB(h->right, val);\n"
    "\n"
    "    if (ehVermelho(h->right) && !ehVermelho(h->left))     h = rotEsqRB(h);\n"
    "    if (ehVermelho(h->left)  && ehVermelho(h->left->left)) h = rotDirRB(h);\n"
    "    if (ehVermelho(h->left)  && ehVermelho(h->right))     inverterCores(h);\n"
    "    return h;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = NULL;\n"
    "    int chaves[] = { 10, 20, 30, 40, 50, 60, 70 };\n"
    "    for (int i = 0; i < 7; i++) {\n"
    "        raiz = inserirRB(raiz, chaves[i]);\n"
    "        definirCor(raiz, false); // A raiz é sempre preta\n"
    "    }\n"
    "    return 0;\n"
    "}\n";

/* 7. Árvore B / Árvore 2-3 (Cisão / Split de Páginas e Promoção de Mediana) */
static const char *PRESET_7_BTREE =
    "// Árvore B / 2-3 (Cisão / Split de Páginas Cheias e Promoção da Chave Mediana)\n"
    "// Veja como páginas saturadas se dividem e promovem a mediana para o nível superior!\n"
    "\n"
    "struct TreeNode* dividirPaginaBTree(struct TreeNode* pagina, int novaChave) {\n"
    "    // Ordena as 3 chaves da página para promover a mediana ao topo\n"
    "    int a = pagina->left ? pagina->left->val : pagina->val;\n"
    "    int b = pagina->val;\n"
    "    int c = novaChave;\n"
    "    if (a > b) { int t = a; a = b; b = t; }\n"
    "    if (b > c) { int t = b; b = c; c = t; }\n"
    "    if (a > b) { int t = a; a = b; b = t; }\n"
    "\n"
    "    struct TreeNode* novaRaiz = new_node(b); // Mediana sobe!\n"
    "    if (pagina->left != NULL) {\n"
    "        pagina->left->val = a;\n"
    "        novaRaiz->left = pagina->left;\n"
    "    } else {\n"
    "        novaRaiz->left = new_node(a);\n"
    "    }\n"
    "    pagina->val = c;\n"
    "    pagina->left = NULL;\n"
    "    novaRaiz->right = pagina;\n"
    "    return novaRaiz;\n"
    "}\n"
    "\n"
    "struct TreeNode* inserirBTree(struct TreeNode* raiz, int chave) {\n"
    "    if (raiz == NULL) return new_node(chave);\n"
    "    // Se é folha com espaço na página, agrupa antes de precisar de Split\n"
    "    if (raiz->left == NULL && raiz->right == NULL) {\n"
    "        if (chave < raiz->val) {\n"
    "            raiz->left = new_node(chave);\n"
    "            return raiz;\n"
    "        }\n"
    "        return dividirPaginaBTree(raiz, chave);\n"
    "    }\n"
    "    if (raiz->right == NULL) {\n"
    "        return dividirPaginaBTree(raiz, chave);\n"
    "    }\n"
    "    if (chave < raiz->val)\n"
    "        raiz->left = inserirBTree(raiz->left, chave);\n"
    "    else\n"
    "        raiz->right = inserirBTree(raiz->right, chave);\n"
    "    return raiz;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = NULL;\n"
    "    int chaves[] = { 20, 10, 30, 40, 50, 25, 60 };\n"
    "    for (int i = 0; i < 7; i++) {\n"
    "        raiz = inserirBTree(raiz, chaves[i]);\n"
    "    }\n"
    "    return 0;\n"
    "}\n";

/* 13. Bug Clássico: Ponteiro Sem NULL (Lixo de Memória) */
static const char *PRESET_9_BUG_GARBAGE =
    "// ARMADILHA 1: Esqueceu de inicializar left e right com NULL após malloc()!\n"
    "\n"
    "struct TreeNode* criarNoSemInicializar(int val) {\n"
    "    struct TreeNode* novo = (struct TreeNode*)malloc(sizeof(struct TreeNode));\n"
    "    novo->val = val;\n"
    "    // ERRO: O malloc() não zera a memória! left e right contêm lixo!\n"
    "    return novo;\n"
    "}\n"
    "\n"
    "struct TreeNode* insert(struct TreeNode* raiz, int val) {\n"
    "    if (raiz == NULL) return criarNoSemInicializar(val);\n"
    "    if (val < raiz->val)\n"
    "        raiz->left = insert(raiz->left, val);\n"
    "    else\n"
    "        raiz->right = insert(raiz->right, val);\n"
    "    return raiz;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = NULL;\n"
    "    raiz = insert(raiz, 50);\n"
    "    raiz = insert(raiz, 30);\n"
    "    return 0;\n"
    "}\n";

/* 10. Bug Clássico: Ciclo em Rotação (Referência Circular) */
static const char *PRESET_10_BUG_CYCLE =
    "// ARMADILHA 2: Erro de ponteiro que cria um Ciclo Estrutural (Árvore Infinita)!\n"
    "\n"
    "struct TreeNode* insertComCiclo(struct TreeNode* raiz, int val) {\n"
    "    if (raiz == NULL) return new_node(val);\n"
    "    if (val < raiz->val) {\n"
    "        raiz->left = insertComCiclo(raiz->left, val);\n"
    "        // BUG: Filho esquerdo passa a apontar de volta para o próprio pai!\n"
    "        if (raiz->left != NULL && val < 35) {\n"
    "            raiz->left->right = raiz;\n"
    "        }\n"
    "    } else {\n"
    "        raiz->right = insertComCiclo(raiz->right, val);\n"
    "    }\n"
    "    return raiz;\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = NULL;\n"
    "    raiz = insertComCiclo(raiz, 50);\n"
    "    raiz = insertComCiclo(raiz, 30);\n"
    "    return 0;\n"
    "}\n";

/* 11. Bug Clássico: Segfault (Desreferenciar NULL) */
static const char *PRESET_11_BUG_SEGFAULT =
    "// ARMADILHA 3: Acessar ->val ou ->left de um ponteiro NULL (Segmentation Fault)!\n"
    "\n"
    "int somarNosSemCasoBase(struct TreeNode* raiz) {\n"
    "    // ERRO: Faltou 'if (raiz == NULL) return 0;' antes de ler raiz->val!\n"
    "    return raiz->val + somarNosSemCasoBase(raiz->left) + somarNosSemCasoBase(raiz->right);\n"
    "}\n"
    "\n"
    "int main(void) {\n"
    "    struct TreeNode* raiz = BUILD_TREE(\"[20, 10, 30]\");\n"
    "    int total = somarNosSemCasoBase(raiz);\n"
    "    (void)total;\n"
    "    return 0;\n"
    "}\n";

void load_code_preset(DebuggerSession *session, int preset_index)
{
    if (!session) return;
    if (preset_index < 0 || preset_index >= PRESET_COUNT) preset_index = 0;
    session->selected_preset = preset_index;

    switch (preset_index) {
        case 0:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_0_SCRATCHPAD);
            snprintf(session->test_input, sizeof(session->test_input), "[10, 5, 15, 2, 7, null, 20]");
            break;
        case 1:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_1_LEETCODE_STD);
            snprintf(session->test_input, sizeof(session->test_input), "[4,1,6,0,2,5,7,null,null,null,3,null,null,null,8]");
            break;
        case 2:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_2_LEETCODE_ARRAY_BST);
            snprintf(session->test_input, sizeof(session->test_input), "[-10, -3, 0, 5, 9, 12, 18]");
            break;
        case 3:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_3_LEETCODE_PRE_IN_ARRAY);
            snprintf(session->test_input, sizeof(session->test_input), "[3, 9, 20, 15, 7]");
            break;
        case 4:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_4_LEETCODE_MERGE_FOREST);
            snprintf(session->test_input, sizeof(session->test_input), "[1, 3, 2, 5]");
            break;
        case 5:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_5_SAME_TREE_FOREST);
            snprintf(session->test_input, sizeof(session->test_input), "[10, 5, 20, 3, 7, null, 30]");
            break;
        case 6:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_6_INVERT_AND_SUBTREE);
            snprintf(session->test_input, sizeof(session->test_input), "[4, 2, 7, 1, 3, 6, 9]");
            break;
        case 7:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_2_SEARCHES);
            snprintf(session->test_input, sizeof(session->test_input), "[50, 30, 70, 20, 40, 60, 80]");
            break;
        case 8:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_3_BST_INSERT_REMOVE);
            snprintf(session->test_input, sizeof(session->test_input), "[50, 30, 70, 20, 40, 60, 80]");
            break;
        case 9:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_4_TRAVERSALS);
            snprintf(session->test_input, sizeof(session->test_input), "[40, 20, 60, 10, 30, 50, 70]");
            break;
        case 10:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_5_AVL);
            snprintf(session->test_input, sizeof(session->test_input), "10, 20, 30, 40, 50, 25");
            break;
        case 11:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_6_RED_BLACK);
            snprintf(session->test_input, sizeof(session->test_input), "10, 20, 30, 40, 50, 60, 70");
            break;
        case 12:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_7_BTREE);
            snprintf(session->test_input, sizeof(session->test_input), "20, 10, 30, 40, 50, 25, 60");
            break;
        case 13:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_9_BUG_GARBAGE);
            snprintf(session->test_input, sizeof(session->test_input), "50, 30, 70");
            break;
        case 14:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_10_BUG_CYCLE);
            snprintf(session->test_input, sizeof(session->test_input), "50, 30, 70");
            break;
        case 15:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_11_BUG_SEGFAULT);
            snprintf(session->test_input, sizeof(session->test_input), "[20, 10, 30]");
            break;
        default:
            snprintf(session->code_buffer, sizeof(session->code_buffer), "%s", PRESET_1_LEETCODE_STD);
            snprintf(session->test_input, sizeof(session->test_input), "[4,1,6,0,2,5,7,null,null,null,3,null,null,null,8]");
            break;
    }
}

/* ============================================================================
 * 2. SESSION LIFECYCLE & PROCESS GROUP CLEANUP
 * ============================================================================ */

void session_init(DebuggerSession *session)
{
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->state          = STATE_IDLE;
    session->gdb_pid        = -1;
    session->stdin_fd       = -1;
    session->stdout_fd      = -1;
    session->stderr_fd      = -1;
    session->speed_slider_t = 0.391f; /* Maps to ~1.00x via 0.05 * 2000^t */
    session->play_speed     = 0.05f * powf(2000.0f, session->speed_slider_t);
    session->auto_play      = false;

    /* Default to Template 1 (LeetCode Padrão with BUILD_TREE) on startup */
    load_code_preset(session, 1);
    diagnostics_clear(&session->diagnostic);
}

static void close_fd_safe(int *fd)
{
    if (fd && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

void gdb_stop_session(DebuggerSession *session)
{
    if (!session) return;

    close_fd_safe(&session->stdin_fd);
    close_fd_safe(&session->stdout_fd);
    close_fd_safe(&session->stderr_fd);

    if (session->gdb_pid > 0) {
        /* Kill entire process group (GDB + student target binary) simultaneously */
        kill(-session->gdb_pid, SIGKILL);
        int status = 0;
        waitpid(session->gdb_pid, &status, WNOHANG);
        for (int retry = 0; retry < 20; retry++) {
            pid_t r = waitpid(session->gdb_pid, &status, WNOHANG);
            if (r != 0) break;
            usleep(1000);
        }
        session->gdb_pid = -1;
    }

    session->watchdog_armed   = false;
    session->waiting_for_step = false;
    session->auto_play        = false;
    session->stdout_len       = 0;
    session->stderr_len       = 0;
}

void session_cleanup(DebuggerSession *session)
{
    if (!session) return;
    gdb_stop_session(session);
    unlink("/tmp/aed_sandbox.c");
    unlink("/tmp/aed_sandbox_bin");
    unlink("/tmp/aed_compile_err.txt");
}

/* ============================================================================
 * 3. INJECTED RUNTIME INTROSPECTION HARNESS & UNIVERSAL LEETCODE ENGINE
 * ============================================================================ */

static const char *INJECTED_HARNESS_PROLOGUE =
    "#define _GNU_SOURCE\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    "#include <string.h>\n"
    "#include <ctype.h>\n"
    "#include <limits.h>\n"
    "#include <unistd.h>\n"
    "#include <errno.h>\n"
    "#include <sys/mman.h>\n"
    "\n"
    "struct TreeNode {\n"
    "    int val;\n"
    "    struct TreeNode *left;\n"
    "    struct TreeNode *right;\n"
    "};\n"
    "\n"
    "/* Global Root Tracker, Forest Roots & Snapshot Serialization Buffer */\n"
    "struct TreeNode *__aed_global_root = NULL;\n"
    "struct TreeNode *__aed_forest_roots[16];\n"
    "int __aed_forest_count = 0;\n"
    "char __aed_snapshot_buf[16384];\n"
    "\n"
    "/* Heap Allocation & Poison Quarantine Tracker */\n"
    "typedef struct {\n"
    "    uintptr_t addr;\n"
    "    size_t    size;\n"
    "    int       state; /* 1 = LIVE, 2 = FREED */\n"
    "} __aed_alloc_entry_t;\n"
    "\n"
    "static __aed_alloc_entry_t __aed_alloc_table[512];\n"
    "static int __aed_alloc_count = 0;\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "void __aed_register_root(struct TreeNode *r) {\n"
    "    if (!r) return;\n"
    "    if (__aed_global_root == NULL) __aed_global_root = r;\n"
    "    for (int i = 0; i < __aed_forest_count; i++) {\n"
    "        if (__aed_forest_roots[i] == r) return;\n"
    "    }\n"
    "    if (__aed_forest_count < 16) {\n"
    "        __aed_forest_roots[__aed_forest_count++] = r;\n"
    "    }\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "void* __aed_malloc(size_t sz) {\n"
    "    void *p = malloc(sz);\n"
    "    if (p) {\n"
    "        /* Poison newly allocated memory with 0xbebebebe... so uninitialized pointers are caught! */\n"
    "        memset(p, 0xBE, sz);\n"
    "        if (__aed_alloc_count < 512) {\n"
    "            __aed_alloc_table[__aed_alloc_count].addr  = (uintptr_t)p;\n"
    "            __aed_alloc_table[__aed_alloc_count].size  = sz;\n"
    "            __aed_alloc_table[__aed_alloc_count].state = 1;\n"
    "            __aed_alloc_count++;\n"
    "        }\n"
    "    }\n"
    "    return p;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "void __aed_free(void *p) {\n"
    "    if (!p) return;\n"
    "    uintptr_t target = (uintptr_t)p;\n"
    "    for (int i = __aed_alloc_count - 1; i >= 0; i--) {\n"
    "        if (__aed_alloc_table[i].addr == target && __aed_alloc_table[i].state == 1) {\n"
    "            __aed_alloc_table[i].state = 2;\n"
    "            memset(p, 0xFD, __aed_alloc_table[i].size);\n"
    "            break;\n"
    "        }\n"
    "    }\n"
    "    free(p);\n"
    "}\n"
    "\n"
    "/* Built-in Helper API (Subsystem E): new_node, bst_insert, BUILD_TREE */\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "struct TreeNode* new_node(int val) {\n"
    "    struct TreeNode *n = (struct TreeNode*)__aed_malloc(sizeof(struct TreeNode));\n"
    "    if (n) {\n"
    "        n->val   = val;\n"
    "        n->left  = NULL;\n"
    "        n->right = NULL;\n"
    "    }\n"
    "    return n;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "struct TreeNode* bst_insert(struct TreeNode *root, int val) {\n"
    "    if (root == NULL) {\n"
    "        struct TreeNode *n = new_node(val);\n"
    "        __aed_register_root(n);\n"
    "        return n;\n"
    "    }\n"
    "    if (val < root->val) {\n"
    "        root->left = bst_insert(root->left, val);\n"
    "    } else if (val > root->val) {\n"
    "        root->right = bst_insert(root->right, val);\n"
    "    }\n"
    "    return root;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "struct TreeNode* __aed_build_tree_level_order(const char *str) {\n"
    "    if (!str) return NULL;\n"
    "    const char *p = str;\n"
    "    while (*p && (isspace((unsigned char)*p) || *p == '[')) p++;\n"
    "    if (!*p || *p == ']') return NULL;\n"
    "\n"
    "    struct TreeNode *q[512];\n"
    "    int qh = 0, qt = 0;\n"
    "    struct TreeNode *root = NULL;\n"
    "\n"
    "    /* Parse root token */\n"
    "    if (strncasecmp(p, \"null\", 4) == 0 || strncmp(p, \"#\", 1) == 0) {\n"
    "        return NULL;\n"
    "    }\n"
    "    char *endptr = NULL;\n"
    "    long rval = strtol(p, &endptr, 10);\n"
    "    if (endptr == p) return NULL;\n"
    "    p = endptr;\n"
    "    root = new_node((int)rval);\n"
    "    q[qt++] = root;\n"
    "\n"
    "    while (*p && qh < qt && qt < 510) {\n"
    "        struct TreeNode *parent = q[qh++];\n"
    "        for (int side = 0; side < 2; side++) {\n"
    "            while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';')) p++;\n"
    "            if (!*p || *p == ']') break;\n"
    "            if (strncasecmp(p, \"null\", 4) == 0) {\n"
    "                p += 4;\n"
    "                while (*p && isalnum((unsigned char)*p)) p++;\n"
    "            } else if (*p == '#') {\n"
    "                p++;\n"
    "            } else {\n"
    "                char *ep = NULL;\n"
    "                long v = strtol(p, &ep, 10);\n"
    "                if (ep != p) {\n"
    "                    p = ep;\n"
    "                    struct TreeNode *child = new_node((int)v);\n"
    "                    if (side == 0) parent->left = child;\n"
    "                    else           parent->right = child;\n"
    "                    if (qt < 512) q[qt++] = child;\n"
    "                } else {\n"
    "                    p++;\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    __aed_register_root(root);\n"
    "    return root;\n"
    "}\n"
    "#define BUILD_TREE(str) __aed_build_tree_level_order(str)\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "int __aed_is_freed_ptr(const void *p) {\n"
    "    uintptr_t u = (uintptr_t)p;\n"
    "    for (int i = __aed_alloc_count - 1; i >= 0; i--) {\n"
    "        if (__aed_alloc_table[i].addr == u) {\n"
    "            return (__aed_alloc_table[i].state == 2) ? 1 : 0;\n"
    "        }\n"
    "    }\n"
    "    return 0;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "int __aed_is_valid_ptr(const void *p) {\n"
    "    if (p == NULL) return 0;\n"
    "    uintptr_t u = (uintptr_t)p;\n"
    "    if (u < 0x10000ULL) return 0;\n"
    "    if ((u & 0x7ULL) != 0) return 0;\n"
    "    if (u == 0xbebebebebebebebeULL || u == 0xebebebebebebebebULL ||\n"
    "        u == 0xfdfdfdfdfdfdfdfdULL || u == 0xcdcdcdcdcdcdcdcdULL ||\n"
    "        u == 0xdeadbeefdeadbeefULL) {\n"
    "        return 0;\n"
    "    }\n"
    "    uint32_t hi = (uint32_t)(u >> 32);\n"
    "    uint32_t lo = (uint32_t)(u & 0xFFFFFFFFU);\n"
    "    if (hi == 0xbebebebeU || lo == 0xbebebebeU ||\n"
    "        hi == 0xfdfdfdfdU || lo == 0xfdfdfdfdU ||\n"
    "        hi == 0xebebebebU || lo == 0xebebebebU) {\n"
    "        return 0;\n"
    "    }\n"
    "    long page_size = sysconf(_SC_PAGESIZE);\n"
    "    if (page_size <= 0) page_size = 4096;\n"
    "    uintptr_t mask = ~((uintptr_t)page_size - 1ULL);\n"
    "    void *page_aligned_ptr = (void *)(u & mask);\n"
    "    errno = 0;\n"
    "    if (msync(page_aligned_ptr, (size_t)page_size, MS_ASYNC) == -1 && errno == ENOMEM) {\n"
    "        return 0;\n"
    "    }\n"
    "    void *end_page = (void *)((u + sizeof(struct TreeNode) - 1ULL) & mask);\n"
    "    if (end_page != page_aligned_ptr) {\n"
    "        errno = 0;\n"
    "        if (msync(end_page, (size_t)page_size, MS_ASYNC) == -1 && errno == ENOMEM) {\n"
    "            return 0;\n"
    "        }\n"
    "    }\n"
    "    return 1;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "int __aed_is_known_alloc(const void *p) {\n"
    "    uintptr_t u = (uintptr_t)p;\n"
    "    for (int i = __aed_alloc_count - 1; i >= 0; i--) {\n"
    "        if (__aed_alloc_table[i].addr == u) return 1;\n"
    "    }\n"
    "    return 0;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "int __aed_can_reach(struct TreeNode *start, struct TreeNode *target) {\n"
    "    if (!start || !target) return 0;\n"
    "    if (start == target) return 1;\n"
    "    struct TreeNode *q[256];\n"
    "    int qh = 0, qt = 0;\n"
    "    q[qt++] = start;\n"
    "    while (qh < qt && qh < 256) {\n"
    "        struct TreeNode *n = q[qh++];\n"
    "        if (!__aed_is_valid_ptr(n) || __aed_is_freed_ptr(n) || !__aed_is_known_alloc(n)) continue;\n"
    "        struct TreeNode *ch[2] = { n->left, n->right };\n"
    "        for (int c = 0; c < 2; c++) {\n"
    "            struct TreeNode *nx = ch[c];\n"
    "            if (!nx) continue;\n"
    "            if (nx == target) return 1;\n"
    "            if (__aed_is_valid_ptr(nx) && !__aed_is_freed_ptr(nx) && __aed_is_known_alloc(nx)) {\n"
    "                int seen = 0;\n"
    "                for (int i = 0; i < qt; i++) {\n"
    "                    if (q[i] == nx) { seen = 1; break; }\n"
    "                }\n"
    "                if (!seen && qt < 256) q[qt++] = nx;\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    return 0;\n"
    "}\n"
    "\n"
    "__attribute__((used, noinline, no_sanitize(\"address\")))\n"
    "const char* __aed_dump_tree(struct TreeNode *root) {\n"
    "    __aed_snapshot_buf[0] = '\\0';\n"
    "    if (root != NULL && !__aed_is_valid_ptr(root)) {\n"
    "        snprintf(__aed_snapshot_buf, sizeof(__aed_snapshot_buf),\n"
    "                 \"INVALID_ROOT:0x%llx;\", (unsigned long long)(uintptr_t)root);\n"
    "        return __aed_snapshot_buf;\n"
    "    }\n"
    "    if (root != NULL && !__aed_is_known_alloc(root)) {\n"
    "        return __aed_snapshot_buf;\n"
    "    }\n"
    "    struct TreeNode *queue[256];\n"
    "    uintptr_t visited_addrs[256];\n"
    "    int head = 0, tail = 0, vcount = 0;\n"
    "    size_t pos = 0;\n"
    "\n"
    "    if (root != NULL) {\n"
    "        queue[tail++] = root;\n"
    "        visited_addrs[vcount++] = (uintptr_t)root;\n"
    "    }\n"
    "\n"
    "    /* Also enqueue any live heap-allocated TreeNode (captures all forest roots & local islands) */\n"
    "    if (root == __aed_global_root) {\n"
    "        for (int a = 0; a < __aed_alloc_count && tail < 256 && vcount < 256; a++) {\n"
    "            if (__aed_alloc_table[a].state == 1 && __aed_alloc_table[a].size == sizeof(struct TreeNode)) {\n"
    "                uintptr_t cand = __aed_alloc_table[a].addr;\n"
    "                int seen = 0;\n"
    "                for (int k = 0; k < vcount; k++) {\n"
    "                    if (visited_addrs[k] == cand) { seen = 1; break; }\n"
    "                }\n"
    "                if (!seen) {\n"
    "                    visited_addrs[vcount++] = cand;\n"
    "                    queue[tail++] = (struct TreeNode *)cand;\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "\n"
    "    while (head < tail && head < 256) {\n"
    "        struct TreeNode *curr = queue[head++];\n"
    "        if (!__aed_is_valid_ptr(curr)) continue;\n"
    "        if (__aed_is_freed_ptr(curr)) {\n"
    "            int w = snprintf(__aed_snapshot_buf + pos, sizeof(__aed_snapshot_buf) - pos,\n"
    "                             \"NODE:0x%llx:0:0x0:0x0:DANGLING_SELF;\",\n"
    "                             (unsigned long long)(uintptr_t)curr);\n"
    "            if (w > 0 && pos + (size_t)w < sizeof(__aed_snapshot_buf)) pos += (size_t)w;\n"
    "            continue;\n"
    "        }\n"
    "        int val = curr->val;\n"
    "        struct TreeNode *L = curr->left;\n"
    "        struct TreeNode *R = curr->right;\n"
    "        char flags[128];\n"
    "        flags[0] = '\\0';\n"
    "\n"
    "        /* Inspect Left Pointer */\n"
    "        if (L != NULL) {\n"
    "            if (!__aed_is_valid_ptr(L)) {\n"
    "                strcat(flags, \"GARBAGE_LEFT|\");\n"
    "            } else if (__aed_is_freed_ptr(L)) {\n"
    "                strcat(flags, \"DANGLING_LEFT|\");\n"
    "            } else if (__aed_can_reach(L, curr)) {\n"
    "                strcat(flags, \"CYCLE_LEFT|\");\n"
    "            } else {\n"
    "                int seen = 0;\n"
    "                for (int i = 0; i < vcount; i++) {\n"
    "                    if (visited_addrs[i] == (uintptr_t)L) { seen = 1; break; }\n"
    "                }\n"
    "                if (!seen && tail < 256 && vcount < 256) {\n"
    "                    visited_addrs[vcount++] = (uintptr_t)L;\n"
    "                    queue[tail++] = L;\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "\n"
    "        /* Inspect Right Pointer */\n"
    "        if (R != NULL) {\n"
    "            if (!__aed_is_valid_ptr(R)) {\n"
    "                strcat(flags, \"GARBAGE_RIGHT|\");\n"
    "            } else if (__aed_is_freed_ptr(R)) {\n"
    "                strcat(flags, \"DANGLING_RIGHT|\");\n"
    "            } else if (__aed_can_reach(R, curr)) {\n"
    "                strcat(flags, \"CYCLE_RIGHT|\");\n"
    "            } else {\n"
    "                int seen = 0;\n"
    "                for (int i = 0; i < vcount; i++) {\n"
    "                    if (visited_addrs[i] == (uintptr_t)R) { seen = 1; break; }\n"
    "                }\n"
    "                if (!seen && tail < 256 && vcount < 256) {\n"
    "                    visited_addrs[vcount++] = (uintptr_t)R;\n"
    "                    queue[tail++] = R;\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "\n"
    "        if (flags[0] == '\\0') {\n"
    "            strcpy(flags, \"OK\");\n"
    "        } else {\n"
    "            size_t flen = strlen(flags);\n"
    "            if (flen > 0 && flags[flen - 1] == '|') flags[flen - 1] = '\\0';\n"
    "        }\n"
    "\n"
    "        int w = snprintf(__aed_snapshot_buf + pos, sizeof(__aed_snapshot_buf) - pos,\n"
    "                         \"NODE:0x%llx:%d:0x%llx:0x%llx:%s;\",\n"
    "                         (unsigned long long)(uintptr_t)curr,\n"
    "                         val,\n"
    "                         (unsigned long long)(uintptr_t)L,\n"
    "                         (unsigned long long)(uintptr_t)R,\n"
    "                         flags);\n"
    "        if (w > 0 && pos + (size_t)w < sizeof(__aed_snapshot_buf)) {\n"
    "            pos += (size_t)w;\n"
    "        } else {\n"
    "            break;\n"
    "        }\n"
    "    }\n"
    "    return __aed_snapshot_buf;\n"
    "}\n"
    "\n"
    "#define malloc(sz) __aed_malloc(sz)\n"
    "#define free(p)    __aed_free(p)\n"
    "\n";

/* Detect whether student code already defines `main(...)` (ignoring comments) */
static bool student_has_main(const char *code)
{
    const char *p = code;
    while (*p) {
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        if (strncmp(p, "main", 4) == 0) {
            bool left_ok = (p == code) || (!isalnum((unsigned char)p[-1]) && p[-1] != '_');
            const char *q = p + 4;
            while (*q && isspace((unsigned char)*q)) q++;
            if (left_ok && *q == '(') return true;
        }
        p++;
    }
    return false;
}

typedef enum {
    SIG_NONE = 0,
    SIG_ONE_TREE,           /* fn(struct TreeNode* root) */
    SIG_TWO_TREES,          /* fn(struct TreeNode* p, struct TreeNode* q) */
    SIG_TREE_AND_INT,       /* fn(struct TreeNode* root, int val) */
    SIG_TREE_AND_INT_PTR,   /* fn(struct TreeNode* root, int* sum) */
    SIG_INT_ARRAY_AND_SIZE  /* fn(int* nums, int numsSize) */
} StudentFuncSigType;

typedef struct {
    char               name[64];
    bool               returns_tree_ptr;
    StudentFuncSigType sig_type;
} ParsedStudentFunc;

/*
 * Smart parser that finds the last non-static top-level C function in the student's buffer
 * so any pasted LeetCode function works even if the student omitted main()!
 */
static bool parse_last_student_function(const char *code, ParsedStudentFunc *out_fn)
{
    if (!code || !out_fn) return false;
    memset(out_fn, 0, sizeof(*out_fn));

    int brace_depth = 0;
    const char *p = code;
    bool found_any = false;

    while (*p) {
        /* Skip single-line comments */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        /* Skip multi-line comments */
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        /* Skip string literals */
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p += 2;
                else p++;
            }
            if (*p == '"') p++;
            continue;
        }

        if (*p == '{') {
            if (brace_depth == 0) {
                /* Look backwards from '{' to find '(' and ')' of function signature */
                const char *close_paren = p - 1;
                while (close_paren > code && isspace((unsigned char)*close_paren)) close_paren--;
                if (*close_paren == ')') {
                    const char *open_paren = close_paren - 1;
                    int pdepth = 1;
                    while (open_paren > code && pdepth > 0) {
                        if (*open_paren == ')') pdepth++;
                        else if (*open_paren == '(') pdepth--;
                        if (pdepth > 0) open_paren--;
                    }
                    if (*open_paren == '(') {
                        /* Extract function name right before '(' */
                        const char *name_end = open_paren - 1;
                        while (name_end > code && isspace((unsigned char)*name_end)) name_end--;
                        const char *name_start = name_end;
                        while (name_start > code && (isalnum((unsigned char)name_start[-1]) || name_start[-1] == '_')) {
                            name_start--;
                        }
                        size_t nlen = (size_t)(name_end - name_start + 1);
                        if (nlen > 0 && nlen < sizeof(out_fn->name)) {
                            char cand_name[64];
                            memcpy(cand_name, name_start, nlen);
                            cand_name[nlen] = '\0';

                            /* Ensure it's not a control keyword or struct definition */
                            if (strcmp(cand_name, "if") != 0 &&
                                strcmp(cand_name, "while") != 0 &&
                                strcmp(cand_name, "for") != 0 &&
                                strcmp(cand_name, "switch") != 0 &&
                                strcmp(cand_name, "TreeNode") != 0) {
                                /* Inspect return type prefix on the same declaration */
                                const char *decl_start = name_start;
                                while (decl_start > code && decl_start[-1] != '\n' && decl_start[-1] != ';' && decl_start[-1] != '}') {
                                    decl_start--;
                                }
                                char prefix[160];
                                size_t plen = (size_t)(name_start - decl_start);
                                if (plen >= sizeof(prefix)) plen = sizeof(prefix) - 1;
                                memcpy(prefix, decl_start, plen);
                                prefix[plen] = '\0';

                                /* Extract parameter list */
                                char params[256];
                                size_t arglen = (size_t)(close_paren - open_paren - 1);
                                if (arglen >= sizeof(params)) arglen = sizeof(params) - 1;
                                memcpy(params, open_paren + 1, arglen);
                                params[arglen] = '\0';

                                if (strstr(prefix, "static") == NULL) {
                                    snprintf(out_fn->name, sizeof(out_fn->name), "%s", cand_name);
                                    out_fn->returns_tree_ptr = (strstr(prefix, "TreeNode") != NULL && strchr(prefix, '*') != NULL);

                                    /* Count occurrences of TreeNode and commas in params */
                                    int comma_count = 0;
                                    for (size_t k = 0; params[k]; k++) {
                                        if (params[k] == ',') comma_count++;
                                    }
                                    if (comma_count == 0 && strstr(params, "TreeNode") != NULL) {
                                        out_fn->sig_type = SIG_ONE_TREE;
                                        found_any = true;
                                    } else if (comma_count == 1 && strstr(params, "TreeNode") != NULL) {
                                        const char *comma = strchr(params, ',');
                                        if (comma && strstr(comma + 1, "TreeNode") != NULL) {
                                            out_fn->sig_type = SIG_TWO_TREES;
                                        } else if (comma && strchr(comma + 1, '*') != NULL) {
                                            out_fn->sig_type = SIG_TREE_AND_INT_PTR;
                                        } else {
                                            out_fn->sig_type = SIG_TREE_AND_INT;
                                        }
                                        found_any = true;
                                    } else if (comma_count == 1 && strstr(params, "int") != NULL &&
                                               (strchr(params, '*') != NULL || strchr(params, '[') != NULL)) {
                                        out_fn->sig_type = SIG_INT_ARRAY_AND_SIZE;
                                        found_any = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            brace_depth++;
        } else if (*p == '}') {
            if (brace_depth > 0) brace_depth--;
        }
        p++;
    }

    return found_any;
}

bool compile_student_code(DebuggerSession *session)
{
    if (!session) return false;
    gdb_stop_session(session);
    session->state = STATE_COMPILING;
    diagnostics_clear(&session->diagnostic);

    FILE *fp = fopen("/tmp/aed_sandbox.c", "w");
    if (!fp) {
        diagnostics_set_compile_error(&session->diagnostic, "Falha ao criar /tmp/aed_sandbox.c");
        session->state = STATE_CRASHED;
        return false;
    }

    /* 1. Emit Injected Runtime Introspection Harness & Universal LeetCode Helper API */
    fputs(INJECTED_HARNESS_PROLOGUE, fp);

    /* 2. Emit #line 1 "student_code.c" for 1:1 GDB <-> UI editor line mapping */
    fputs("#line 1 \"student_code.c\"\n", fp);
    fputs(session->code_buffer, fp);
    fputs("\n", fp);

    /* 3. Smart main() Resolution & Failsafe (Subsystem E) */
    if (!student_has_main(session->code_buffer)) {
        ParsedStudentFunc fn;
        bool has_fn = parse_last_student_function(session->code_buffer, &fn);

        session->diagnostic.auto_main_warning = true;
        snprintf(session->diagnostic.auto_main_func, sizeof(session->diagnostic.auto_main_func),
                 "%s", has_fn ? fn.name : "BUILD_TREE");

        /* Sanitize test_input string for C literal embedding */
        char safe_input[MAX_TEST_INPUT];
        size_t si = 0;
        for (size_t i = 0; session->test_input[i] && si < sizeof(safe_input) - 2; i++) {
            char c = session->test_input[i];
            if (c != '"' && c != '\\' && c != '\n' && c != '\r') {
                safe_input[si++] = c;
            }
        }
        safe_input[si] = '\0';
        if (safe_input[0] == '\0') {
            strcpy(safe_input, "[4,1,6,0,2,5,7,null,null,null,3,null,null,null,8]");
        }

        fprintf(fp, "\n#line 10000 \"aed_harness_driver.c\"\n");
        fprintf(fp, "int main(void) {\n");

        if (has_fn && fn.sig_type == SIG_ONE_TREE) {
            fprintf(fp, "    struct TreeNode *root = BUILD_TREE(\"%s\");\n", safe_input);
            fprintf(fp, "    __aed_global_root = root;\n");
            if (fn.returns_tree_ptr) {
                fprintf(fp, "    root = %s(root);\n", fn.name);
                fprintf(fp, "    __aed_global_root = root;\n");
            } else {
                fprintf(fp, "    %s(root);\n", fn.name);
            }
        } else if (has_fn && fn.sig_type == SIG_TWO_TREES) {
            fprintf(fp, "    struct TreeNode *arvore1 = BUILD_TREE(\"%s\");\n", safe_input);
            fprintf(fp, "    struct TreeNode *arvore2 = BUILD_TREE(\"%s\");\n", safe_input);
            fprintf(fp, "    __aed_global_root = arvore1;\n");
            fprintf(fp, "    %s(arvore1, arvore2);\n", fn.name);
        } else if (has_fn && fn.sig_type == SIG_TREE_AND_INT_PTR) {
            fprintf(fp, "    struct TreeNode *root = BUILD_TREE(\"%s\");\n", safe_input);
            fprintf(fp, "    int acumulador = 0;\n");
            fprintf(fp, "    __aed_global_root = root;\n");
            fprintf(fp, "    %s(root, &acumulador);\n", fn.name);
        } else if (has_fn && fn.sig_type == SIG_TREE_AND_INT) {
            /* Check if test_input is bracketed level-order or comma-separated insertion list */
            const char *trim = safe_input;
            while (*trim && isspace((unsigned char)*trim)) trim++;
            if (*trim == '[') {
                fprintf(fp, "    struct TreeNode *raiz = BUILD_TREE(\"%s\");\n", safe_input);
                fprintf(fp, "    __aed_global_root = raiz;\n");
                if (fn.returns_tree_ptr) {
                    fprintf(fp, "    raiz = %s(raiz, 25);\n", fn.name);
                    fprintf(fp, "    __aed_global_root = raiz;\n");
                } else {
                    fprintf(fp, "    %s(raiz, 25);\n", fn.name);
                }
            } else {
                int vals[128];
                int vcount = 0;
                const char *p = safe_input;
                while (*p && vcount < 128) {
                    while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';')) p++;
                    if (!*p) break;
                    char *endptr = NULL;
                    long v = strtol(p, &endptr, 10);
                    if (endptr != p) {
                        vals[vcount++] = (int)v;
                        p = endptr;
                    } else {
                        p++;
                    }
                }
                if (vcount == 0) {
                    int defaults[] = { 50, 30, 70, 20, 40, 60, 80 };
                    for (int i = 0; i < 7; i++) vals[vcount++] = defaults[i];
                }
                fprintf(fp, "    struct TreeNode *raiz = NULL;\n");
                fprintf(fp, "    int valores[%d] = { ", vcount);
                for (int i = 0; i < vcount; i++) {
                    fprintf(fp, "%d%s", vals[i], (i + 1 < vcount) ? ", " : "");
                }
                fprintf(fp, " };\n");
                fprintf(fp, "    for (int i = 0; i < %d; i++) {\n", vcount);
                if (fn.returns_tree_ptr) {
                    fprintf(fp, "        raiz = %s(raiz, valores[i]);\n", fn.name);
                } else {
                    fprintf(fp, "        %s(raiz, valores[i]);\n", fn.name);
                }
                fprintf(fp, "        __aed_global_root = raiz;\n");
                fprintf(fp, "    }\n");
            }
        } else if (has_fn && fn.sig_type == SIG_INT_ARRAY_AND_SIZE) {
            int vals[128];
            int vcount = 0;
            const char *p = safe_input;
            while (*p && vcount < 128) {
                while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';' || *p == '[' || *p == ']')) p++;
                if (!*p) break;
                char *endptr = NULL;
                long v = strtol(p, &endptr, 10);
                if (endptr != p) {
                    vals[vcount++] = (int)v;
                    p = endptr;
                } else {
                    p++;
                }
            }
            if (vcount == 0) {
                int defaults[] = { -10, -3, 0, 5, 9, 12, 18 };
                for (int i = 0; i < 7; i++) vals[vcount++] = defaults[i];
            }
            fprintf(fp, "    int nums[%d] = { ", vcount);
            for (int i = 0; i < vcount; i++) {
                fprintf(fp, "%d%s", vals[i], (i + 1 < vcount) ? ", " : "");
            }
            fprintf(fp, " };\n");
            if (fn.returns_tree_ptr) {
                fprintf(fp, "    struct TreeNode *root = %s(nums, %d);\n", fn.name, vcount);
                fprintf(fp, "    __aed_global_root = root;\n");
            } else {
                fprintf(fp, "    %s(nums, %d);\n", fn.name, vcount);
            }
        } else {
            fprintf(fp, "    struct TreeNode *root = BUILD_TREE(\"%s\");\n", safe_input);
            fprintf(fp, "    __aed_global_root = root;\n");
        }

        fprintf(fp, "    return 0;\n");
        fprintf(fp, "}\n");
    }

    fclose(fp);

    /* 4. Run GCC with AddressSanitizer first */
    session->asan_enabled = true;
    int rc = system(
        "LC_ALL=C gcc -std=c11 -g3 -O0 -fsanitize=address -fno-omit-frame-pointer "
        "/tmp/aed_sandbox.c -o /tmp/aed_sandbox_bin 2> /tmp/aed_compile_err.txt"
    );

    if (rc != 0) {
        char err_buf[2048] = { 0 };
        FILE *ef = fopen("/tmp/aed_compile_err.txt", "r");
        if (ef) {
            size_t n = fread(err_buf, 1, sizeof(err_buf) - 1, ef);
            err_buf[n] = '\0';
            fclose(ef);
        }

        if (strstr(err_buf, "libasan") != NULL || strstr(err_buf, "-lasan") != NULL) {
            /* Fallback compilation without -fsanitize=address when host lacks libasan package */
            session->asan_enabled = false;
            rc = system(
                "LC_ALL=C gcc -std=c11 -g3 -O0 -fno-omit-frame-pointer "
                "/tmp/aed_sandbox.c -o /tmp/aed_sandbox_bin 2> /tmp/aed_compile_err.txt"
            );
        }
    }

    if (rc != 0) {
        char err_buf[2048] = { 0 };
        FILE *ef = fopen("/tmp/aed_compile_err.txt", "r");
        if (ef) {
            size_t n = fread(err_buf, 1, sizeof(err_buf) - 1, ef);
            err_buf[n] = '\0';
            fclose(ef);
        }
        diagnostics_set_compile_error(&session->diagnostic, err_buf);
        session->state = STATE_CRASHED;
        return false;
    }

    return true;
}

/* ============================================================================
 * 4. GDB/MI SUBPROCESS SPAWNER & NON-BLOCKING IPC
 * ============================================================================ */

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void gdb_send_command(DebuggerSession *session, const char *cmd)
{
    if (!session || session->stdin_fd < 0 || !cmd) return;
    size_t len = strlen(cmd);
    ssize_t w = write(session->stdin_fd, cmd, len);
    (void)w;
    if (len == 0 || cmd[len - 1] != '\n') {
        ssize_t wn = write(session->stdin_fd, "\n", 1);
        (void)wn;
    }
}

static void arm_watchdog(DebuggerSession *session)
{
    clock_gettime(CLOCK_MONOTONIC, &session->last_cmd_timestamp);
    session->watchdog_armed = true;
}

bool gdb_start_session(DebuggerSession *session)
{
    if (!session) return false;
    gdb_stop_session(session);

    int in_pipe[2]  = { -1, -1 };
    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };

    if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        close_fd_safe(&in_pipe[0]);  close_fd_safe(&in_pipe[1]);
        close_fd_safe(&out_pipe[0]); close_fd_safe(&out_pipe[1]);
        close_fd_safe(&err_pipe[0]); close_fd_safe(&err_pipe[1]);
        session->state = STATE_CRASHED;
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close_fd_safe(&in_pipe[0]);  close_fd_safe(&in_pipe[1]);
        close_fd_safe(&out_pipe[0]); close_fd_safe(&out_pipe[1]);
        close_fd_safe(&err_pipe[0]); close_fd_safe(&err_pipe[1]);
        session->state = STATE_CRASHED;
        return false;
    }

    if (pid == 0) {
        /* CHILD PROCESS: Process group leader + automatic death if parent UI dies */
        setpgid(0, 0);
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        /* Enforce ASAN_OPTIONS to prevent LeakSanitizer ptrace deadlock under GDB */
        setenv("ASAN_OPTIONS", "detect_leaks=0:abort_on_error=1:halt_on_error=1:print_legend=0", 1);
        setenv("LC_ALL", "C", 1);

        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close(in_pipe[0]);  close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);

        execlp("gdb", "gdb", "--interpreter=mi3", "--quiet", "--nx", "/tmp/aed_sandbox_bin", (char *)NULL);
        _exit(127);
    }

    /* PARENT PROCESS */
    setpgid(pid, pid);
    close(in_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[1]);

    session->gdb_pid   = pid;
    session->stdin_fd  = in_pipe[1];
    session->stdout_fd = out_pipe[0];
    session->stderr_fd = err_pipe[0];

    set_nonblocking(session->stdout_fd);
    set_nonblocking(session->stderr_fd);

    /* Reset snapshot state */
    memset(&session->snapshot, 0, sizeof(session->snapshot));
    session->snapshot.current_executing_line = 1;
    session->step_bounce_guard = 0;

    /* Configure GDB environment & initial breakpoint */
    gdb_send_command(session, "-gdb-set environment ASAN_OPTIONS detect_leaks=0:abort_on_error=1:halt_on_error=1");
    gdb_send_command(session, "-gdb-set pagination off");
    gdb_send_command(session, "-gdb-set confirm off");
    gdb_send_command(session, "-gdb-set print elements 0");
    gdb_send_command(session, "-gdb-set print repeats 0");
    gdb_send_command(session, "-break-insert main");
    gdb_send_command(session, "-exec-run");

    session->state = STATE_STEPPING;
    session->waiting_for_step = true;
    arm_watchdog(session);
    return true;
}

void gdb_request_step(DebuggerSession *session)
{
    if (!session || session->gdb_pid <= 0) return;
    if (session->state != STATE_PAUSED && session->state != STATE_RUNNING) return;

    session->state = STATE_STEPPING;
    session->waiting_for_step = true;
    arm_watchdog(session);
    gdb_send_command(session, "-exec-step");
}

/* ============================================================================
 * 5. SYNCHRONOUS SHORT-BURST GDB EXPRESSION QUERY HELPER (NON-BLOCKING BOUNDED)
 * ============================================================================ */

static bool gdb_query_sync(DebuggerSession *session, const char *cmd, char *reply_out, size_t reply_sz, int timeout_ms)
{
    if (!session || session->stdin_fd < 0 || session->stdout_fd < 0) return false;
    reply_out[0] = '\0';

    gdb_send_command(session, cmd);

    struct timespec start_ts, now_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    size_t len = 0;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double elapsed_ms = (double)(now_ts.tv_sec - start_ts.tv_sec) * 1000.0 +
                            (double)(now_ts.tv_nsec - start_ts.tv_nsec) / 1.0e6;
        if (elapsed_ms >= (double)timeout_ms) break;

        struct pollfd pfd = { .fd = session->stdout_fd, .events = POLLIN, .revents = 0 };
        int pr = poll(&pfd, 1, 2);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            char tmp[2048];
            ssize_t n = read(session->stdout_fd, tmp, sizeof(tmp) - 1);
            if (n > 0) {
                tmp[n] = '\0';
                size_t copy = (size_t)n;
                if (len + copy >= reply_sz) {
                    copy = reply_sz - len - 1;
                }
                if (copy > 0) {
                    memcpy(reply_out + len, tmp, copy);
                    len += copy;
                    reply_out[len] = '\0';
                }
                if (strstr(reply_out, "^done") != NULL || strstr(reply_out, "^error") != NULL) {
                    return true;
                }
            }
        }
    }
    return (strstr(reply_out, "^done") != NULL);
}

/* ============================================================================
 * 6. SNAPSHOT SERIALIZATION PARSER, FOREST ROOTS & GRAPH MERGER
 * ============================================================================ */

static VisualNode* find_or_add_node(
    TreeSnapshot       *new_snap,
    const TreeSnapshot *old_snap,
    uint64_t            addr,
    bool                is_detached
)
{
    for (int i = 0; i < new_snap->node_count; i++) {
        if (new_snap->nodes[i].addr == addr) {
            if (!is_detached) {
                new_snap->nodes[i].is_detached = false;
            }
            return &new_snap->nodes[i];
        }
    }
    if (new_snap->node_count >= MAX_TREE_NODES) return NULL;

    VisualNode *node = &new_snap->nodes[new_snap->node_count++];
    memset(node, 0, sizeof(*node));
    node->addr        = addr;
    node->alpha       = 1.0f;
    node->scale       = 1.0f;
    node->is_detached = is_detached;
    node->forest_id   = -1;

    /* Preserve previous position for smooth 60 FPS Vector2Lerp animation */
    bool existed = false;
    if (old_snap) {
        for (int i = 0; i < old_snap->node_count; i++) {
            if (old_snap->nodes[i].addr == addr) {
                node->current_pos     = old_snap->nodes[i].current_pos;
                node->target_pos      = old_snap->nodes[i].target_pos;
                node->highlight_timer = old_snap->nodes[i].highlight_timer;
                existed = true;
                break;
            }
        }
    }

    if (!existed) {
        /* Newly allocated nodes start in the top-left Staging Zone and pop in */
        node->current_pos        = (Vector2){ -260.0f, -20.0f };
        node->target_pos         = node->current_pos;
        node->is_newly_allocated = true;
        node->highlight_timer    = 1.0f;
    }

    return node;
}

static void parse_dump_tree_output(
    const char         *gdb_reply,
    TreeSnapshot       *new_snap,
    const TreeSnapshot *old_snap,
    bool                is_detached_tree
)
{
    const char *p = strstr(gdb_reply, "NODE:");
    while (p != NULL) {
        const char *semi = strchr(p, ';');
        char record[256];
        size_t rlen = semi ? (size_t)(semi - p) : strlen(p);
        if (rlen >= sizeof(record)) rlen = sizeof(record) - 1;
        memcpy(record, p, rlen);
        record[rlen] = '\0';

        unsigned long long addr = 0, laddr = 0, raddr = 0;
        int val = 0;
        char flags[128] = { 0 };

        if (sscanf(record, "NODE:0x%llx:%d:0x%llx:0x%llx:%127[^;\"]",
                   &addr, &val, &laddr, &raddr, flags) == 5 && addr != 0) {
            VisualNode *n = find_or_add_node(new_snap, old_snap, (uint64_t)addr, is_detached_tree);
            if (n) {
                if (n->val != val && !n->is_newly_allocated) {
                    n->highlight_timer = 1.0f;
                }
                n->val        = val;
                n->left_addr  = (uint64_t)laddr;
                n->right_addr = (uint64_t)raddr;

                if (strstr(flags, "DANGLING_SELF")) {
                    n->is_dangling = true;
                }

                /* Left edge state */
                if (strstr(flags, "CYCLE_LEFT")) {
                    n->left_state = EDGE_CYCLE;
                } else if (strstr(flags, "GARBAGE_LEFT")) {
                    n->left_state = EDGE_GARBAGE_PTR;
                } else if (strstr(flags, "DANGLING_LEFT")) {
                    n->left_state = EDGE_DANGLING;
                } else if (laddr != 0) {
                    n->left_state = EDGE_OK;
                } else {
                    n->left_state = EDGE_NONE;
                }

                /* Right edge state */
                if (strstr(flags, "CYCLE_RIGHT")) {
                    n->right_state = EDGE_CYCLE;
                } else if (strstr(flags, "GARBAGE_RIGHT")) {
                    n->right_state = EDGE_GARBAGE_PTR;
                } else if (strstr(flags, "DANGLING_RIGHT")) {
                    n->right_state = EDGE_DANGLING;
                } else if (raddr != 0) {
                    n->right_state = EDGE_OK;
                } else {
                    n->right_state = EDGE_NONE;
                }
            }
        }

        if (!semi) break;
        p = strstr(semi + 1, "NODE:");
    }
}

static void add_forest_root_unique(TreeSnapshot *snap, uint64_t addr)
{
    if (!snap || addr == 0) return;
    for (int i = 0; i < snap->forest_root_count; i++) {
        if (snap->forest_roots[i] == addr) return;
    }
    if (snap->forest_root_count < MAX_FOREST_ROOTS) {
        snap->forest_roots[snap->forest_root_count++] = addr;
    }
}

static bool is_root_like_var_name(const char *name)
{
    if (!name) return false;
    return (strcmp(name, "raiz") == 0 ||
            strcmp(name, "root") == 0 ||
            strcmp(name, "no") == 0 ||
            strcmp(name, "p") == 0 ||
            strcmp(name, "q") == 0 ||
            strncmp(name, "arvore", 6) == 0 ||
            strncmp(name, "tree", 4) == 0 ||
            strncmp(name, "root", 4) == 0 ||
            strncmp(name, "r1", 2) == 0 ||
            strncmp(name, "r2", 2) == 0);
}

static void refresh_runtime_snapshot(DebuggerSession *session, int line, const char *func_name)
{
    TreeSnapshot old_snap = session->snapshot;
    TreeSnapshot new_snap;
    memset(&new_snap, 0, sizeof(new_snap));
    new_snap.current_executing_line = line;
    snprintf(new_snap.current_func, sizeof(new_snap.current_func), "%s", func_name ? func_name : "main");

    char reply[32768];

    /* 1. Query global root tracked by harness (`__aed_global_root`) and all live heap nodes */
    if (gdb_query_sync(session, "-data-evaluate-expression \"(unsigned long long)__aed_global_root\"",
                       reply, sizeof(reply), 25)) {
        const char *v = strstr(reply, "value=\"");
        if (v) {
            new_snap.active_root_addr = (uint64_t)strtoull(v + 7, NULL, 0);
        }
    }
    if (gdb_query_sync(session, "-data-evaluate-expression \"__aed_dump_tree(__aed_global_root)\"",
                       reply, sizeof(reply), 35)) {
        parse_dump_tree_output(reply, &new_snap, &old_snap, (new_snap.active_root_addr == 0));
    }

    /* 2. Query `-stack-list-variables --simple-values` to discover all `struct TreeNode *` in scope */
    if (gdb_query_sync(session, "-stack-list-variables --simple-values", reply, sizeof(reply), 30)) {
        const char *cursor = reply;
        while ((cursor = strstr(cursor, "{name=\"")) != NULL) {
            const char *end_brace = strchr(cursor, '}');
            if (!end_brace) break;

            char item[512];
            size_t ilen = (size_t)(end_brace - cursor);
            if (ilen >= sizeof(item)) ilen = sizeof(item) - 1;
            memcpy(item, cursor, ilen);
            item[ilen] = '\0';

            if (strstr(item, "struct TreeNode *") != NULL) {
                char var_name[64] = { 0 };
                const char *np = strstr(item, "name=\"");
                if (np) {
                    np += 6;
                    size_t k = 0;
                    while (np[k] && np[k] != '"' && k < sizeof(var_name) - 1) {
                        var_name[k] = np[k];
                        k++;
                    }
                    var_name[k] = '\0';
                }

                uint64_t ptr_val = 0;
                const char *vp = strstr(item, "value=\"");
                if (vp) {
                    vp += 7;
                    ptr_val = (uint64_t)strtoull(vp, NULL, 0);
                }

                if (var_name[0] != '\0' && strncmp(var_name, "__aed", 5) != 0 &&
                    new_snap.local_var_count < MAX_LOCAL_VARS) {
                    LocalPointerVar *lv = &new_snap.local_vars[new_snap.local_var_count++];
                    snprintf(lv->name, sizeof(lv->name), "%s", var_name);
                    lv->addr    = ptr_val;
                    lv->is_null = (ptr_val == 0);

                    if (ptr_val != 0) {
                        char dump_cmd[160];
                        char dump_reply[32768];
                        snprintf(dump_cmd, sizeof(dump_cmd),
                                 "-data-evaluate-expression \"__aed_dump_tree(%s)\"", var_name);
                        if (gdb_query_sync(session, dump_cmd, dump_reply, sizeof(dump_reply), 35)) {
                            bool is_root_like = is_root_like_var_name(var_name);
                            if (strstr(dump_reply, "INVALID_ROOT:") != NULL) {
                                if (ptr_val == 0xbebebebebebebebeULL ||
                                    ptr_val == 0xfdfdfdfdfdfdfdfdULL ||
                                    is_root_like) {
                                    lv->is_garbage = true;
                                }
                                lv->is_valid_node = false;
                            } else if (strstr(dump_reply, "NODE:") != NULL) {
                                lv->is_garbage    = false;
                                lv->is_valid_node = true;
                                if (new_snap.active_root_addr == 0 && is_root_like) {
                                    new_snap.active_root_addr = ptr_val;
                                }
                                bool detached = (ptr_val != new_snap.active_root_addr);
                                parse_dump_tree_output(dump_reply, &new_snap, &old_snap, detached);
                            }
                        }
                    }
                }
            }
            cursor = end_brace + 1;
        }
    }

    /* 3. Discover Multi-Tree Forest Roots (all nodes with in-degree 0 along valid tree edges) */
    bool has_parent[MAX_TREE_NODES] = { false };
    for (int i = 0; i < new_snap.node_count; i++) {
        const VisualNode *n = &new_snap.nodes[i];
        if (n->left_state == EDGE_OK && n->left_addr != 0) {
            for (int j = 0; j < new_snap.node_count; j++) {
                if (new_snap.nodes[j].addr == n->left_addr) has_parent[j] = true;
            }
        }
        if (n->right_state == EDGE_OK && n->right_addr != 0) {
            for (int j = 0; j < new_snap.node_count; j++) {
                if (new_snap.nodes[j].addr == n->right_addr) has_parent[j] = true;
            }
        }
    }

    /* If active_root_addr was rotated down, promote its parent to active_root_addr */
    for (int hop = 0; hop < new_snap.node_count; hop++) {
        bool promoted = false;
        for (int i = 0; i < new_snap.node_count; i++) {
            const VisualNode *n = &new_snap.nodes[i];
            if ((n->left_state == EDGE_OK && n->left_addr == new_snap.active_root_addr) ||
                (n->right_state == EDGE_OK && n->right_addr == new_snap.active_root_addr)) {
                new_snap.active_root_addr = n->addr;
                promoted = true;
                break;
            }
        }
        if (!promoted) break;
    }

    if (new_snap.active_root_addr != 0) {
        /* Verify active_root_addr is still a live node in new_snap */
        bool root_alive = false;
        for (int i = 0; i < new_snap.node_count; i++) {
            if (new_snap.nodes[i].addr == new_snap.active_root_addr && !new_snap.nodes[i].is_dangling) {
                root_alive = true;
                break;
            }
        }
        if (root_alive) {
            add_forest_root_unique(&new_snap, new_snap.active_root_addr);
        } else {
            new_snap.active_root_addr = 0;
        }
    }

    /* Add any other in-degree 0 node that is either pointed to by a root-like variable or has children */
    for (int i = 0; i < new_snap.node_count; i++) {
        const VisualNode *n = &new_snap.nodes[i];
        if (!has_parent[i] && !n->is_dangling) {
            bool has_children = (n->left_state == EDGE_OK && n->left_addr != 0) ||
                                (n->right_state == EDGE_OK && n->right_addr != 0);
            bool pointed_by_root_var = false;
            bool pointed_only_by_novo = false;
            for (int v = 0; v < new_snap.local_var_count; v++) {
                if (new_snap.local_vars[v].addr == n->addr) {
                    if (is_root_like_var_name(new_snap.local_vars[v].name)) {
                        pointed_by_root_var = true;
                    }
                    if (strcmp(new_snap.local_vars[v].name, "novo") == 0 ||
                        strcmp(new_snap.local_vars[v].name, "temp") == 0) {
                        pointed_only_by_novo = true;
                    }
                }
            }
            if (has_children || (pointed_by_root_var && !pointed_only_by_novo)) {
                add_forest_root_unique(&new_snap, n->addr);
                if (new_snap.active_root_addr == 0) {
                    new_snap.active_root_addr = n->addr;
                }
            }
        }
    }

    /* 4. Reconcile detached flags: nodes reachable from any forest root are attached; others are Staging Zone islands */
    for (int i = 0; i < new_snap.node_count; i++) {
        new_snap.nodes[i].is_detached = true;
        new_snap.nodes[i].forest_id   = -1;
    }

    for (int r = 0; r < new_snap.forest_root_count; r++) {
        uint64_t raddr = new_snap.forest_roots[r];
        for (int i = 0; i < new_snap.node_count; i++) {
            if (new_snap.nodes[i].addr == raddr) {
                new_snap.nodes[i].is_detached = false;
                new_snap.nodes[i].forest_id   = r;
            }
        }
    }

    for (int pass = 0; pass < new_snap.node_count; pass++) {
        for (int i = 0; i < new_snap.node_count; i++) {
            VisualNode *n = &new_snap.nodes[i];
            if (!n->is_detached) {
                for (int j = 0; j < new_snap.node_count; j++) {
                    if ((n->left_state == EDGE_OK && new_snap.nodes[j].addr == n->left_addr) ||
                        (n->right_state == EDGE_OK && new_snap.nodes[j].addr == n->right_addr)) {
                        new_snap.nodes[j].is_detached = false;
                        new_snap.nodes[j].forest_id   = n->forest_id;
                    }
                }
            }
        }
    }

    /* Highlight nodes directly pointed to by local variables */
    for (int i = 0; i < new_snap.node_count; i++) {
        new_snap.nodes[i].is_active_focus = false;
        for (int v = 0; v < new_snap.local_var_count; v++) {
            if (new_snap.local_vars[v].addr == new_snap.nodes[i].addr) {
                new_snap.nodes[i].is_active_focus = true;
                break;
            }
        }
    }

    session->snapshot = new_snap;
    diagnostics_inspect_snapshot(&session->snapshot, &session->diagnostic, &session->state);
}

/* ============================================================================
 * 7. NON-BLOCKING GDB/MI EVENT POLLING & WATCHDOG
 * ============================================================================ */

static void extract_mi_field(const char *line, const char *key, char *out, size_t out_sz)
{
    out[0] = '\0';
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", key);
    const char *p = strstr(line, pattern);
    if (!p) return;
    p += strlen(pattern);
    size_t i = 0;
    while (p[i] && p[i] != '"' && i < out_sz - 1) {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
}

static void handle_gdb_mi_line(DebuggerSession *session, const char *line)
{
    if (!line || line[0] == '\0') return;

    if (strncmp(line, "*stopped", 8) == 0) {
        session->watchdog_armed   = false;
        session->waiting_for_step = false;

        char reason[64]   = { 0 };
        char sig_name[64] = { 0 };
        char file_str[128]= { 0 };
        char func_str[64] = { 0 };
        char line_str[32] = { 0 };

        extract_mi_field(line, "reason", reason, sizeof(reason));
        extract_mi_field(line, "signal-name", sig_name, sizeof(sig_name));
        extract_mi_field(line, "file", file_str, sizeof(file_str));
        extract_mi_field(line, "func", func_str, sizeof(func_str));
        extract_mi_field(line, "line", line_str, sizeof(line_str));

        int src_line = atoi(line_str);

        /* 1. Check normal program exit */
        if (strcmp(reason, "exited-normally") == 0 || strcmp(reason, "exited") == 0) {
            session->state     = STATE_FINISHED;
            session->auto_play = false;

            /* Escalate to Critical Orange Garbage Error if a node in the final tree still retains an uninitialized pointer */
            if (session->diagnostic.error_type == ERR_NONE) {
                for (int i = 0; i < session->snapshot.node_count; i++) {
                    const VisualNode *n = &session->snapshot.nodes[i];
                    if (n->left_state == EDGE_GARBAGE_PTR || n->right_state == EDGE_GARBAGE_PTR) {
                        uint64_t bad_ptr = (n->left_state == EDGE_GARBAGE_PTR) ? n->left_addr : n->right_addr;
                        session->diagnostic.has_uninit_ptr_warning = false;
                        diagnostics_set_runtime_trap(
                            &session->diagnostic,
                            ERR_SIGSEGV_GARBAGE,
                            session->snapshot.current_executing_line,
                            bad_ptr,
                            n->addr,
                            "A execucao terminou, mas a arvore final ainda contem ponteiros nao inicializados (lixo de memoria)."
                        );
                        break;
                    }
                }
            }

            if (session->diagnostic.error_type != ERR_NONE) {
                session->state = STATE_CRASHED;
            } else {
                session->diagnostic.has_uninit_ptr_warning = false;
                snprintf(
                    session->diagnostic.pedagogical_title,
                    sizeof(session->diagnostic.pedagogical_title),
                    "[CONCLUÍDO] Execução Finalizada com Sucesso (%d nós na memória Heap)",
                    session->snapshot.node_count
                );
                snprintf(
                    session->diagnostic.pedagogical_explanation,
                    sizeof(session->diagnostic.pedagogical_explanation),
                    "Todas as instruções de main() terminaram sem erros de memória! "
                    "Use o mouse no Canvas à direita para arrastar (Pan) e dar Zoom na árvore final."
                );
            }
            gdb_stop_session(session);
            return;
        }

        /* 2. Check fatal runtime signals (SIGSEGV / SIGABRT from ASan) */
        if (strcmp(reason, "signal-received") == 0) {
            session->state     = STATE_CRASHED;
            session->auto_play = false;
            session->diagnostic.has_uninit_ptr_warning = false;
            int fault_line = (src_line > 0 && src_line < 9000)
                             ? src_line
                             : session->snapshot.current_executing_line;
            session->snapshot.current_executing_line = fault_line;

            if (strcmp(sig_name, "SIGSEGV") == 0 || strcmp(sig_name, "SIGBUS") == 0) {
                bool has_garbage = false;
                uint64_t bad_addr = 0;
                uint64_t parent_addr = 0;
                for (int i = 0; i < session->snapshot.local_var_count; i++) {
                    if (session->snapshot.local_vars[i].is_garbage) {
                        has_garbage = true;
                        bad_addr = session->snapshot.local_vars[i].addr;
                        break;
                    }
                }
                if (!has_garbage) {
                    for (int i = 0; i < session->snapshot.node_count; i++) {
                        const VisualNode *n = &session->snapshot.nodes[i];
                        if (n->left_state == EDGE_GARBAGE_PTR) {
                            has_garbage = true;
                            bad_addr    = n->left_addr;
                            parent_addr = n->addr;
                            break;
                        }
                        if (n->right_state == EDGE_GARBAGE_PTR) {
                            has_garbage = true;
                            bad_addr    = n->right_addr;
                            parent_addr = n->addr;
                            break;
                        }
                    }
                }
                if (has_garbage) {
                    diagnostics_set_runtime_trap(
                        &session->diagnostic,
                        ERR_SIGSEGV_GARBAGE,
                        fault_line,
                        bad_addr,
                        parent_addr,
                        line
                    );
                } else {
                    diagnostics_set_runtime_trap(
                        &session->diagnostic,
                        ERR_SIGSEGV_NULL,
                        fault_line,
                        0,
                        0,
                        line
                    );
                }
            } else if (strcmp(sig_name, "SIGABRT") == 0) {
                if (strstr(session->stderr_buf, "heap-use-after-free") != NULL) {
                    diagnostics_set_runtime_trap(
                        &session->diagnostic,
                        ERR_ASAN_UAF,
                        fault_line,
                        0,
                        0,
                        session->stderr_buf
                    );
                } else {
                    diagnostics_set_runtime_trap(
                        &session->diagnostic,
                        ERR_ASAN_OOB,
                        fault_line,
                        0,
                        0,
                        session->stderr_buf
                    );
                }
            }
            return;
        }

        /* 3. If stopped inside our injected harness (e.g. BUILD_TREE, new_node, bst_insert) or glibc, auto-step back */
        bool in_student_code = (strstr(file_str, "student_code.c") != NULL) && (src_line > 0 && src_line < 9000);
        if (!in_student_code) {
            session->step_bounce_guard++;
            if (session->step_bounce_guard < 96) {
                session->state = STATE_STEPPING;
                session->waiting_for_step = true;
                arm_watchdog(session);
                if (strstr(file_str, "aed_harness_driver.c") != NULL) {
                    /* Step into the student's function from auto-generated main() */
                    gdb_send_command(session, "-exec-step");
                } else {
                    /* Finish internal helper (BUILD_TREE / new_node / libc) and return to student caller */
                    gdb_send_command(session, "-exec-finish");
                }
                return;
            }
        }

        /* 4. Stopped cleanly inside student_code.c! */
        session->step_bounce_guard = 0;
        session->state = session->auto_play ? STATE_RUNNING : STATE_PAUSED;
        refresh_runtime_snapshot(session, src_line, func_str);

        /* Pause auto-play automatically if a pedagogical trap (Cycle/Garbage/Dangling) was spotted */
        if (session->diagnostic.error_type != ERR_NONE) {
            session->auto_play = false;
            session->state     = STATE_PAUSED;
        }
    }
}

void gdb_poll_and_parse(DebuggerSession *session)
{
    if (!session || session->gdb_pid <= 0) return;

    struct pollfd pfds[2];
    pfds[0].fd      = session->stdout_fd;
    pfds[0].events  = POLLIN;
    pfds[0].revents = 0;
    pfds[1].fd      = session->stderr_fd;
    pfds[1].events  = POLLIN;
    pfds[1].revents = 0;

    int ready = poll(pfds, 2, 0);
    if (ready <= 0) return;

    /* Read non-blocking stderr (captures ASan reports) */
    if (pfds[1].revents & POLLIN) {
        char tmp[1024];
        ssize_t n = read(session->stderr_fd, tmp, sizeof(tmp) - 1);
        if (n > 0) {
            tmp[n] = '\0';
            size_t rem = sizeof(session->stderr_buf) - session->stderr_len - 1;
            size_t copy = ((size_t)n < rem) ? (size_t)n : rem;
            if (copy > 0) {
                memcpy(session->stderr_buf + session->stderr_len, tmp, copy);
                session->stderr_len += copy;
                session->stderr_buf[session->stderr_len] = '\0';
            }
        }
    }

    /* Read non-blocking stdout (GDB/MI stream) */
    if (pfds[0].revents & POLLIN) {
        char tmp[4096];
        ssize_t n = read(session->stdout_fd, tmp, sizeof(tmp) - 1);
        if (n > 0) {
            tmp[n] = '\0';
            size_t rem = sizeof(session->stdout_buf) - session->stdout_len - 1;
            size_t copy = ((size_t)n < rem) ? (size_t)n : rem;
            if (copy > 0) {
                memcpy(session->stdout_buf + session->stdout_len, tmp, copy);
                session->stdout_len += copy;
                session->stdout_buf[session->stdout_len] = '\0';
            }

            /* Process complete newline-delimited lines */
            char *line_start = session->stdout_buf;
            char *newline    = NULL;
            while ((newline = strchr(line_start, '\n')) != NULL) {
                *newline = '\0';
                if (newline > line_start && newline[-1] == '\r') {
                    newline[-1] = '\0';
                }
                handle_gdb_mi_line(session, line_start);
                line_start = newline + 1;
                if (session->gdb_pid <= 0) return;
            }

            size_t leftover = strlen(line_start);
            if (leftover > 0 && line_start != session->stdout_buf) {
                memmove(session->stdout_buf, line_start, leftover + 1);
            } else if (leftover == 0) {
                session->stdout_buf[0] = '\0';
            }
            session->stdout_len = leftover;
        }
    }
}

void watchdog_check(DebuggerSession *session)
{
    if (!session || session->gdb_pid <= 0 || !session->watchdog_armed) return;
    if (session->state != STATE_STEPPING && session->state != STATE_RUNNING) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (double)(now.tv_sec - session->last_cmd_timestamp.tv_sec) +
                     (double)(now.tv_nsec - session->last_cmd_timestamp.tv_nsec) / 1.0e9;

    if (elapsed > WATCHDOG_TIMEOUT_SEC) {
        int last_line = session->snapshot.current_executing_line;
        gdb_stop_session(session);
        session->state = STATE_TIMEOUT;
        diagnostics_set_runtime_trap(
            &session->diagnostic,
            ERR_INFINITE_LOOP,
            last_line,
            0,
            0,
            "Watchdog POSIX (CLOCK_MONOTONIC > 1.5s): Processo encerrado com kill(-gdb_pid, SIGKILL)."
        );
    }
}
