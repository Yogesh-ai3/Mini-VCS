#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(name) _mkdir(name)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define make_dir(name) mkdir(name, 0777)
#endif

#define HASH_SIZE 101

/* ===================== DATA STRUCTURES ===================== */
typedef struct FileVersion
{
    char *filename;
    struct FileVersion *next;
} FileVersion;

typedef struct Commit
{
    char *hash;
    char *message;
    char *timestamp;
    FileVersion *files;
    struct Commit **parents;
    int parent_count;
    struct Commit *next; // for hash table chaining
} Commit;

typedef struct Branch
{
    char *name;
    Commit *head;
    struct Branch *next;
} Branch;

/* ===================== GLOBALS ===================== */
Commit *commitTable[HASH_SIZE];
Branch *branches = NULL;
Branch *currentBranch = NULL;
void saveCommitFiles(Commit *c);

/* ===================== HELPERS ===================== */
unsigned int hashFunc(const char *str)
{
    unsigned int hash = 0;
    while (*str)
        hash = (hash * 31) + *str++;
    return hash % HASH_SIZE;
}

char *getTimeStamp()
{
    time_t now = time(NULL);
    char *t = ctime(&now);
    t[strlen(t) - 1] = '\0';
    return strdup(t);
}

void insertCommit(Commit *c)
{
    unsigned int idx = hashFunc(c->hash);
    c->next = commitTable[idx];
    commitTable[idx] = c;
}

Commit *findCommit(const char *hash)
{
    unsigned int idx = hashFunc(hash);
    Commit *curr = commitTable[idx];
    while (curr)
    {
        if (strcmp(curr->hash, hash) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/* ===================== FILE HANDLING ===================== */

void restoreFilesFromCommit(Commit *c)
{
    if (!c)
        return;
    char folder[256];
    snprintf(folder, sizeof(folder), ".minigit/%s", c->hash);
    for (FileVersion *f = c->files; f; f = f->next)
    {
        char srcpath[256];
        snprintf(srcpath, sizeof(srcpath), "%s/%s", folder, f->filename);
        FILE *src = fopen(srcpath, "r");
        FILE *dst = fopen(f->filename, "w");
        if (!src || !dst)
            continue;

        int ch;
        while ((ch = fgetc(src)) != EOF)
            fputc(ch, dst);

        fclose(src);
        fclose(dst);
    }
    printf("📂 Restored files from commit %s\n", c->hash);
}

void addFile(FileVersion **head, const char *fname)
{
    FileVersion *node = malloc(sizeof(FileVersion));
    node->filename = strdup(fname);
    node->next = *head;
    *head = node;
    printf("📄 Tracked file '%s'\n", fname);
}

/* ===================== COMMIT SYSTEM ===================== */
Commit *createCommit(const char *msg, Commit **parents, int parent_count, FileVersion *files)
{
    Commit *c = malloc(sizeof(Commit));
    c->message = strdup(msg);
    c->timestamp = getTimeStamp();
    if (parent_count > 0)
        c->parents = malloc(sizeof(Commit *) * parent_count);
    else
        c->parents = NULL;

    for (int i = 0; i < parent_count; i++)
        c->parents[i] = parents[i];
    c->parent_count = parent_count;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s-%ld", msg, time(NULL));
    c->hash = strdup(buf);

    c->files = files;
    c->next = NULL;
    insertCommit(c);
    return c;
}

void commit(const char *msg)
{
    Commit *parent[1] = {currentBranch->head};
    FileVersion *files = currentBranch->head ? currentBranch->head->files : NULL;
    Commit *newCommit = createCommit(msg, parent, 1, files);
    saveCommitFiles(newCommit);
    currentBranch->head = newCommit;
    printf(" Commit created: %s\n", newCommit->hash);
}

/* ===================== BRANCH / MERGE ===================== */
void createBranch(const char *name)
{
    Branch *b = malloc(sizeof(Branch));
    b->name = strdup(name);
    b->head = currentBranch->head;
    b->next = branches;
    branches = b;
    printf("🌿 Branch '%s' created at commit %s\n", name, b->head->hash);
}

void checkout(const char *name)
{
    Branch *b = branches;
    while (b)
    {
        if (strcmp(b->name, name) == 0)
        {
            currentBranch = b;
            printf("🔁 Switched to branch '%s'\n", name);
            restoreFilesFromCommit(currentBranch->head);
            return;
        }
        b = b->next;
    }
    printf("❌ Branch not found.\n");
}
FileVersion *mergeFileLists(FileVersion *a, FileVersion *b)
{
    FileVersion *result = NULL, *tail = NULL;

    for (FileVersion *f = a; f; f = f->next)
    {
        FileVersion *node = malloc(sizeof(FileVersion));
        node->filename = strdup(f->filename);
        node->next = NULL;
        if (!result)
            result = tail = node;
        else
        {
            tail->next = node;
            tail = node;
        }
    }

    for (FileVersion *f = b; f; f = f->next)
    {
        FileVersion *temp = result;
        int exists = 0;
        while (temp)
        {
            if (strcmp(temp->filename, f->filename) == 0)
            {
                exists = 1;

                {
                    printf("⚠️  Conflict in '%s' — keeping version from first branch.\n", f->filename);
                }
                break;
            }
            temp = temp->next;
        }

        if (!exists)
        {
            FileVersion *node = malloc(sizeof(FileVersion));
            node->filename = strdup(f->filename);
            node->next = NULL;
            tail->next = node;
            tail = node;
        }
    }

    return result;
}

void mergeBranches(const char *name1, const char *name2, const char *msg)
{
    Branch *b1 = branches, *b2 = branches;
    while (b1 && strcmp(b1->name, name1) != 0)
        b1 = b1->next;
    while (b2 && strcmp(b2->name, name2) != 0)
        b2 = b2->next;
    if (!b1 || !b2)
    {
        printf("❌ Branch not found.\n");
        return;
    }

    Commit *parents[2] = {b1->head, b2->head};
    FileVersion *mergedFiles = mergeFileLists(b1->head->files, b2->head->files);
    Commit *merged = createCommit(msg, parents, 2, mergedFiles);
    saveCommitFiles(merged);
    currentBranch->head = merged;
    printf(" Merged '%s' and '%s' into new commit %s\n", name1, name2, merged->hash);
}

/* ===================== DISPLAY FUNCTIONS ===================== */
void listBranches(Branch *branches, Branch *current)
{
    printf("\nBranches:\n");
    for (Branch *b = branches; b; b = b->next)
        printf("%s%s\n", (b == current ? "* " : "  "), b->name);
}

void showBranchCommits()
{
    Branch *b = branches;
    printf("\n=== Branch Commit Graph ===\n");
    while (b)
    {
        printf("\n🌿 %s\n", b->name);
        Commit *c = b->head;
        while (c)
        {
            printf("   ├── %s (%s)\n", c->hash, c->message);
            if (c->parent_count > 1)
            {
                printf("   │    ↳ merge of ");
                for (int i = 0; i < c->parent_count; i++)
                    printf("[%s]%s", c->parents[i]->hash,
                           i == c->parent_count - 1 ? "" : ", ");
                printf("\n");
            }
            c = (c->parent_count > 0) ? c->parents[0] : NULL;
        }
        b = b->next;
    }
    printf("-----------------------------\n");
}

void logCommits(Commit *c)
{
    if (!c)
        return;
    printf("\nCommit: %s\nMessage: %s\nDate: %s\n", c->hash, c->message, c->timestamp);
    printf("Files tracked:\n");
    for (FileVersion *f = c->files; f; f = f->next)
        printf("   📄 %s\n", f->filename);
    printf("-------------------------------\n");
    for (int i = 0; i < c->parent_count; i++)
        logCommits(c->parents[i]);
}

/* ===================== SAVE COMMITS TO FILE ===================== */
void saveCommitFiles(Commit *c)
{
    char folder[256];
    snprintf(folder, sizeof(folder), ".minigit/%s", c->hash);
    make_dir(".minigit");
    make_dir(folder);

    for (FileVersion *f = c->files; f; f = f->next)
    {
        char dest[256];
        snprintf(dest, sizeof(dest), "%s/%s", folder, f->filename);
        FILE *src = fopen(f->filename, "r");
        FILE *dst = fopen(dest, "w");
        if (!src || !dst)
            continue;

        int ch;
        while ((ch = fgetc(src)) != EOF)
            fputc(ch, dst);

        fclose(src);
        fclose(dst);
    }
}

/* ===================== INIT REPO ===================== */
void initRepo()
{
    for (int i = 0; i < HASH_SIZE; i++)
        commitTable[i] = NULL;

    Commit *init = createCommit("Initial commit", NULL, 0, NULL);
    Branch *master = malloc(sizeof(Branch));
    master->name = strdup("master");
    master->head = init;
    master->next = NULL;
    branches = master;
    currentBranch = master;
    printf("📦 Repository initialized (branch: master)\n");
}

/* ===================== CLI MENU ===================== */
void menu()
{
    int choice;
    char msg[100], fname[100], content[100], hash[200], bname[100], b1[100], b2[100];

    while (1)
    {
        printf("\n=== MiniGit Menu ===\n");
        printf("1. Add File\n2. Commit\n3. Log\n4. Create Branch\n5. Checkout Branch\n6. Merge Branches\n7. Search Commit by Hash\n8. List Branches\n9. Show Commits by Branch\n10.Exit\nChoice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice)
        {
        case 1:
            printf("Enter filename: ");
            fgets(fname, sizeof(fname), stdin);
            fname[strcspn(fname, "\n")] = 0;
            addFile(&currentBranch->head->files, fname);
            printf("📄 File '%s' added.\n", fname);
            break;

        case 2:
            printf("Enter commit message: ");
            fgets(msg, sizeof(msg), stdin);
            msg[strcspn(msg, "\n")] = 0;
            commit(msg);
            break;

        case 3:
            printf("==== Commit Log ====\n");
            logCommits(currentBranch->head);
            break;

        case 4:
            printf("Enter branch name: ");
            fgets(bname, sizeof(bname), stdin);
            bname[strcspn(bname, "\n")] = 0;
            createBranch(bname);
            break;

        case 5:
            printf("Enter branch to checkout: ");
            fgets(bname, sizeof(bname), stdin);
            bname[strcspn(bname, "\n")] = 0;
            checkout(bname);
            break;

        case 6:
            printf("Enter first branch: ");
            fgets(b1, sizeof(b1), stdin);
            b1[strcspn(b1, "\n")] = 0;
            printf("Enter second branch: ");
            fgets(b2, sizeof(b2), stdin);
            b2[strcspn(b2, "\n")] = 0;
            printf("Enter merge message: ");
            fgets(msg, sizeof(msg), stdin);
            msg[strcspn(msg, "\n")] = 0;
            mergeBranches(b1, b2, msg);
            break;

        case 7:
            printf("Enter commit hash: ");
            fgets(hash, sizeof(hash), stdin);
            hash[strcspn(hash, "\n")] = 0;
            Commit *found = findCommit(hash);
            if (found)
            {
                printf("\n🔎 Commit found!\nMessage: %s\nDate: %s\nFiles:\n", found->message, found->timestamp);
                printf("\nSnapshots saved under: .minigit/%s/\n", found->hash);
            }
            else
            {
                printf("❌ Commit not found.\n");
            }
            break;

        case 8:
            listBranches(branches, currentBranch);
            break;

        case 9:
            showBranchCommits();
            break;

        case 10:
            printf("Exiting MiniGit...\n");
            return;

        default:
            printf("Invalid choice.\n");
        }
    }
}

/* ===================== MAIN ===================== */
int main()
{
    initRepo();
    menu();
    return 0;
}
