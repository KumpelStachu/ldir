#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/xattr.h>
#include <sys/acl.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define FLAGS          \
  FLAG(a, all)         \
  FLAG(l, long_format) \
  FLAG(c, color)       \
  FLAG(d, directories) \
  FLAG(f, no_sort)     \
  FLAG(s, size_sort)   \
  FLAG(t, type_sort)   \
  FLAG(r, reverse)     \
  FLAG(h, human)       \
  FLAG(H, help)

typedef int (*Comparator)(const void *, const void *);

typedef struct Options
{
  int dirfd;
  char *name;
  char *path;
  uint32_t total;
  struct max
  {
    uint16_t name;
    uint16_t user;
    uint16_t group;
    uint16_t size;
    uint16_t link;
    uint16_t window;
  } max;
  struct Flags
  {
#define FLAG(_, x) bool x;
    FLAGS
#undef FLAG
  } flags;
} Options;

typedef struct ListItem
{
  struct dirent *entry;
  struct stat stats;
} ListItem;

typedef struct UidNameCache
{
  uid_t uid;
  char *name;
  struct UidNameCache *next;
} UidNameCache;

void print_usage(FILE *stream, char *name)
{
  if (name == NULL)
    name = "ldir";

  fprintf(stream, "Usage: %s [options] [path]\n", name);
  fprintf(stream, "Options:\n");
  fprintf(stream,
#define FLAG(x, y) "  -" #x ",  --" #y "\n"
          FLAGS
#undef FLAG
  );
}

Options parse_options(int argc, char **argv)
{
  Options options = {0};

  options.path = ".";
  options.flags.color = isatty(STDOUT_FILENO);

  while (argc-- > 0)
  {
    char *arg = *argv++;
    if (arg[0] == '-')
    {
      for (char *c = arg + 1; *c; c++)
      {
        switch (*c)
        {
#define FLAG(x, y)          \
  case #x[0]:               \
    options.flags.y = true; \
    break;
          FLAGS
#undef FLAG
        case '-':
          if (c[1] == '\0')
          {
            options.name = *argv;
            return options;
          }
#define FLAG(x, y)                 \
  else if (strcmp(c + 1, #y) == 0) \
      options.flags.y = true,      \
      c += sizeof(#y) - 1;
          FLAGS
#undef FLAG
          else
          {
            fprintf(stderr, "Unknown option: --%s\n", c + 1);
            print_usage(stderr, options.name);
            exit(1);
          }
          break;
        default:
          fprintf(stderr, "Unknown option: -%c\n", *c);
          print_usage(stderr, options.name);
          exit(1);
          break;
        }
      }
    }
    else if (options.name == NULL)
    {
      options.name = arg;
    }
    else
    {
      options.path = arg;
    }
  }

  options.flags.color &= isatty(STDOUT_FILENO);

  return options;
}

char *get_user(uid_t uid)
{
  static UidNameCache *cache = NULL;
  for (UidNameCache *c = cache; c != NULL; c = c->next)
  {
    if (c->uid == uid)
      return c->name;
  }

  struct passwd *pw = getpwuid(uid);
  if (pw == NULL)
  {
    perror("getpwuid");
    return NULL;
  }

  UidNameCache *new = malloc(sizeof(UidNameCache));
  new->uid = uid;
  new->name = pw->pw_name;
  new->next = cache;
  cache = new;

  return new->name;
}

char *get_group(uid_t uid)
{
  static UidNameCache *cache = NULL;
  for (UidNameCache *c = cache; c != NULL; c = c->next)
  {
    if (c->uid == uid)
      return c->name;
  }

  struct group *gr = getgrgid(uid);
  if (gr == NULL)
  {
    perror("getgrgid");
    return NULL;
  }

  UidNameCache *new = malloc(sizeof(UidNameCache));
  new->uid = uid;
  new->name = gr->gr_name;
  new->next = cache;
  cache = new;

  return new->name;
}

const char *item_color(ListItem *item)
{
  switch (item->entry->d_type)
  {
  case DT_DIR:
    return "\x1b[1;36m";
  case DT_REG:
    return item->stats.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH) ? "\x1b[31m" : "\x1b[0m";
  case DT_LNK:
    return "\x1b[35m";
  case DT_FIFO:
    return "\x1b[1;33m";
  case DT_SOCK:
    return "\x1b[1;35m";
  case DT_BLK:
    return "\x1b[46;30m";
  case DT_CHR:
    return "\x1b[43;30m";
  default:
    return "\x1b[0m";
  }
}

char type_char(uint8_t type)
{
  switch (type)
  {
  case DT_FIFO:
    return 'p';
  case DT_CHR:
    return 'c';
  case DT_DIR:
    return 'd';
  case DT_BLK:
    return 'b';
  case DT_REG:
    return '-';
  case DT_LNK:
    return 'l';
  case DT_SOCK:
    return 's';
  case DT_WHT:
    return 'w';
  default:
    return '?';
  }
}

char special_char(Options *options, ListItem *item)
{
  if (item->entry->d_type != DT_REG && item->entry->d_type != DT_LNK && item->entry->d_type != DT_DIR)
    return ' ';

  int fd = openat(options->dirfd, item->entry->d_name, O_RDONLY | O_NOFOLLOW);

  // TODO: Fix /{DT_LNK}
  int xattr = flistxattr(fd, NULL, 0, 0);
  if (xattr > 0)
  {
    close(fd);
    return '@';
  }

  // TODO: Fix ~/.Thrash
  acl_t acl = acl_get_fd_np(fd, ACL_TYPE_EXTENDED);
  acl_entry_t acl_entry;
  if (acl && acl_get_entry(acl, ACL_FIRST_ENTRY, &acl_entry) != -1)
  {
    acl_free(acl);
    close(fd);
    return '+';
  }

  return ' ';
}

char *human_size(uint64_t size)
{
  static char buffer[16];
  static const char *suffixes = "BKMGTPE";
  double s = size;
  int i = 0;

  while (s >= 1024 && i < (int)sizeof(suffixes) - 1)
  {
    s /= 1024;
    i++;
  }

  snprintf(buffer, sizeof(buffer), "%.*f%c", i > 0 && s < 9.9, s, suffixes[i]);

  return buffer;
}

int li_alphasort(const void *va, const void *vb)
{
  const ListItem *a = va, *b = vb;

  return strcoll(a->entry->d_name, b->entry->d_name);
}

int li_sizesort(const void *va, const void *vb)
{
  const ListItem *a = va, *b = vb;

  int diff = b->stats.st_size - a->stats.st_size;
  if (diff)
    return diff;

  return li_alphasort(a, b);
}

int li_typesort(const void *va, const void *vb)
{
  const ListItem *a = va, *b = vb;

  int diff = a->entry->d_type - b->entry->d_type;
  if (diff)
    return diff;

  return li_alphasort(a, b);
}

Comparator get_comparator(Options options)
{
  if (options.flags.size_sort)
    return li_sizesort;
  if (options.flags.type_sort)
    return li_typesort;
  return li_alphasort;
}

void print_basic(Options *options, ListItem *item)
{
  static uint16_t posx = 0;

  posx += options->max.name + 1;
  if (posx > options->max.window)
  {
    putchar('\n');
    posx = options->max.name + 1;
  }

  printf("%s%s%s%*c",
         options->flags.color ? item_color(item) : "",
         item->entry->d_name,
         options->flags.color ? "\x1b[0m" : "",
         -options->max.name + (int)strlen(item->entry->d_name) - 1, ' ');
}

void print_long(Options *options, ListItem *item)
{
  char *time = ctime(&item->stats.st_mtime) + 4;
  time[12] = '\0';

  char size[options->max.size + 1];
  if (!options->flags.human)
    snprintf(size, options->max.size + 1, "%llu", item->stats.st_size);

  // TODO: refactor to use putchar?
  printf("%c%c%c%c%c%c%c%c%c%c%c %*d %*s  %*s  %*s %s %s%s%s",
         type_char(item->entry->d_type),
  // TODO: stiky bit, setuid, setgid
#define MODE(x) item->stats.st_mode &S_I##x ? (#x[0] - 'A' + 'a') : '-'
#define TYPE(x) MODE(R##x), MODE(W##x), MODE(X##x)
         TYPE(USR), TYPE(GRP), TYPE(OTH),
#undef TYPE
#undef MODE
         special_char(options, item),
         options->max.link, item->stats.st_nlink,
         -options->max.user, get_user(item->stats.st_uid),
         -options->max.group, get_group(item->stats.st_gid),
         options->max.size, options->flags.human ? human_size(item->stats.st_size) : size,
         time,
         options->flags.color ? item_color(item) : "",
         item->entry->d_name,
         options->flags.color ? "\x1b[0m" : "");

  if (item->entry->d_type == DT_LNK)
  {
    char link[1024];
    ssize_t len = readlinkat(options->dirfd, item->entry->d_name, link, sizeof(link));
    if (len == -1)
    {
      perror("readlinkat");
      return;
    }
    link[len] = '\0';
    printf(" -> %s", link);
  }

  putchar('\n');
}

bool skip_entry(Options *options, struct dirent *entry)
{
  if (!options->flags.all && entry->d_name[0] == '.')
    return true;

  if (options->flags.directories && entry->d_type != DT_DIR)
    return true;

  return false;
}

int main(int argc, char **argv)
{
  Options options = parse_options(argc, argv);
  struct dirent **entries;

  if (options.flags.help)
  {
    print_usage(stdout, options.name);
    return 0;
  }

  DIR *dir = opendir(options.path);
  if (dir == NULL)
  {
    perror("opendir");
    return 1;
  }
  options.dirfd = dirfd(dir);

  int entries_count = scandir(options.path, &entries, NULL, NULL);
  if (entries_count == -1)
  {
    perror("scandir");
    return 1;
  }

  ListItem *items = malloc(entries_count * sizeof(ListItem));
  for (int i = 0; i < entries_count; i++)
  {
    struct dirent *entry = entries[i];
    items[i].entry = entries[i];

    if (skip_entry(&options, entry))
      continue;

    options.max.name = MAX(options.max.name, entry->d_namlen);

    struct stat *stats = &items[i].stats;
    if (fstatat(options.dirfd, entries[i]->d_name, stats, AT_SYMLINK_NOFOLLOW) == -1)
    {
      perror("fstatat");
      return 1;
    }

    options.total += stats->st_blocks;
    options.max.user = MAX(options.max.user, strlen(get_user(stats->st_uid)));
    options.max.group = MAX(options.max.group, strlen(get_group(stats->st_gid)));
    options.max.link = MAX(options.max.link, snprintf(NULL, 0, "%d", stats->st_nlink));
    options.max.size = MAX(options.max.size, options.flags.human ? strlen(human_size(stats->st_size)) : snprintf(NULL, 0, "%llu", stats->st_size)); //
  }

  options.max.size += options.flags.human;

  if (options.flags.long_format && !options.flags.directories)
  {
    printf("total %d\n", options.total);
  }
  else
  {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    options.max.window = ws.ws_col;
  }

  if (!options.flags.no_sort)
    qsort(items, entries_count, sizeof(ListItem), get_comparator(options));

  for (int j = 0; j < entries_count; j++)
  {
    int i = options.flags.reverse ? entries_count - j - 1 : j;

    if (skip_entry(&options, items[i].entry))
      continue;

    if (options.flags.long_format)
      print_long(&options, items + i);
    else
      print_basic(&options, items + i);
  }

  if (!options.flags.long_format)
    putchar('\n');

  return 0;
}
