#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HASH_SIZE 101

/* ===================== DATA STRUCTURES ===================== */
typedef struct FileVersion
{
    char *filename;
    char *content;
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
FileVersion *stagingArea = NULL;


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
FileVersion *copyFiles(FileVersion *head)
{
    if (!head)
        return NULL;
    FileVersion *newHead = NULL, *tail = NULL;
    while (head)
    {
        FileVersion *node = malloc(sizeof(FileVersion));
        node->filename = strdup(head->filename);
        node->content = strdup(head->content);
        node->next = NULL;
        if (!newHead)
            newHead = tail = node;
        else
        {
            tail->next = node;
            tail = node;
        }
        head = head->next;
    }
    return newHead;
}

void addFile(FileVersion **head, const char *fname, const char *content)
{
    FileVersion *node = malloc(sizeof(FileVersion));
    node->filename = strdup(fname);
    node->content = strdup(content);
    node->next = *head;
    *head = node;
}

/* ===================== COMMIT SYSTEM ===================== */
Commit *createCommit(const char *msg, Commit **parents, int parent_count, FileVersion *files)
{
    Commit *c = malloc(sizeof(Commit));
    c->message = strdup(msg);
    c->timestamp = getTimeStamp();
    c->parents = malloc(sizeof(Commit *) * parent_count);
    for (int i = 0; i < parent_count; i++)
        c->parents[i] = parents[i];
    c->parent_count = parent_count;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s-%ld", msg, time(NULL));
    c->hash = strdup(buf);

    c->files = copyFiles(files);
    c->next = NULL;
    insertCommit(c);
    return c;
}

void commit(const char *msg)
{
    Commit *parent[1] = {currentBranch->head};
    FileVersion *files = currentBranch->head ? copyFiles(currentBranch->head->files) : NULL;
    Commit *newCommit = createCommit(msg, parent, 1, files);
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
            return;
        }
        b = b->next;
    }
    printf("❌ Branch not found.\n");
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
    FileVersion *files = copyFiles(b1->head->files);
    Commit *merged = createCommit(msg, parents, 2, files);
    currentBranch->head = merged;
    printf(" Merged '%s' and '%s' into new commit %s\n", name1, name2, merged->hash);
}

/* ===================== DISPLAY FUNCTIONS ===================== */
void showFiles(FileVersion *f)
{
    while (f)
    {
        printf("    %s -> \"%s\"\n", f->filename, f->content);
        f = f->next;
    }
}

void logCommits(Commit *c)
{
    if (!c)
        return;
    printf("\nCommit: %s\nMessage: %s\nDate: %s\n", c->hash, c->message, c->timestamp);
    printf("Files:\n");
    showFiles(c->files);
    printf("-------------------------------\n");
    for (int i = 0; i < c->parent_count; i++)
        logCommits(c->parents[i]);
}

/* ===================== SAVE COMMITS TO FILE ===================== */
void saveCommitsToFile()
{
    FILE *fp = fopen("commits.txt", "w");
    if (fp==NULL)
    {
        printf("❌ Could not open file.\n");
        return;
    }

    for (int i = 0; i < HASH_SIZE; i++)
    {
        Commit *curr = commitTable[i];
        while (curr)
        {
            fprintf(fp, "HASH: %s\nMESSAGE: %s\nDATE: %s\nFILES:\n", curr->hash, curr->message, curr->timestamp);
            FileVersion *f = curr->files;
            while (f)
            {
                fprintf(fp, "  %s -> %s\n", f->filename, f->content);
                f = f->next;
            }
            fprintf(fp, "-----------------------\n");
            curr = curr->next;
        }
    }

    fclose(fp);
    printf("💾 Commits saved to commits.txt\n");
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
        printf("1. Add File\n2. Commit\n3. Log\n4. Create Branch\n5. Checkout Branch\n6. Merge Branches\n7. Search Commit by Hash\n8. Save Commits to File\n9. Exit\nChoice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice)
        {
        case 1:
            printf("Enter filename: ");
            fgets(fname, sizeof(fname), stdin);
            fname[strcspn(fname, "\n")] = 0;
            printf("Enter content: ");
            fgets(content, sizeof(content), stdin);
            content[strcspn(content, "\n")] = 0;
            addFile(&currentBranch->head->files, fname, content);
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
                showFiles(found->files);
            }
            else
            {
                printf("❌ Commit not found.\n");
            }
            break;

        case 8:
            saveCommitsToFile();
            break;

        case 9:
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
