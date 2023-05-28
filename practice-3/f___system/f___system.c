/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall f___system.c `pkg-config fuse3 --cflags --libs` -o f__system
 *
 * ## Source code ##
 * \include f___system.c
 */


#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

struct file_list;

struct file {
	char *name, *data;
	struct file_list * edges;
	struct file * father;
	int size;
	int is_directory;
};
struct file * root, *hello, *bot1, *bot2, *botdir, *botdir1, *botdir2;

struct file_list {
	struct file * file;
	char * name;
	struct file_list * next;
};

struct file_list * file_list_cons(struct file * file, char * name, struct file_list * next) {
	struct file_list * ret = (struct file_list *) malloc(sizeof(struct file_list));
	ret->file = file;
	ret->name = name;
	ret->next = next;
	return ret;
}

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

struct file_list * init_file_edges(struct file * father, struct file * self) {
	return file_list_cons(father, "..", file_list_cons(self, ".", NULL));
}

struct file * create_dir(struct file * father, char * name) {
	struct file * ret = (struct file *) malloc(sizeof(struct file));
	ret->edges = init_file_edges(father, ret);
	ret->size = 0;
	ret->data = NULL;
	ret->name = name;
	ret->is_directory = 1;
	ret->father = father;
	father->edges = file_list_cons(ret, ret->name, father->edges);
	return ret;
}

struct file * create_reg(struct file * father, char * name, char *data) {
	struct file * ret = (struct file *) malloc(sizeof(struct file));
	ret->edges = NULL;
	ret->size = strlen(data);
	ret->data = (char*) malloc(ret->size + 3);
	memcpy(ret->data, data, ret->size);
	ret->data[ret->size] = 0;
	ret->name = name;
	ret->is_directory = 0;
	ret->father = father;
	father->edges = file_list_cons(ret, ret->name, father->edges);
	return ret;
}

static void *vexoben_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	printf("vexoben_init\n");
	(void) conn;
	cfg->kernel_cache = 1;
	root = (struct file * ) malloc(sizeof(struct file));
	root->edges = init_file_edges(root, root);
	root->size = 0;
	root->data = NULL;
	root->is_directory = 1;
	root->name = "root";
	root->father = root;

	hello = (struct file *) malloc(sizeof(struct file));
	hello->edges = NULL;
	hello->size = strlen(options.contents);
	hello->data = (char*) options.contents;
	hello->name = (char*) options.filename;
	hello->is_directory = 0;
	hello->father = root;
	root->edges = file_list_cons(hello, hello->name, root->edges);

	botdir = create_dir(root, "chatbox");
	botdir1 = create_dir(botdir, "bot1");
	bot2 = create_reg(botdir1, "bot2", "bot1: hello bot2\nbot2: hello bot1\n");
	botdir2 = create_dir(botdir, "bot2");
	bot1 = create_reg(botdir2, "bot1", "bot1: hello bot2\nbot2: hello bot1\n");
	
	return NULL;
}

int get_next_name(const char *path, int pos, int len) {
	++pos;
	while (pos < len && path[pos] != '/') ++pos;
	return pos;
}

struct file * get_file(struct file * cur_file, char * name) {
	struct file_list * tmp;
	for (tmp = cur_file->edges; tmp != NULL; tmp = tmp->next) {
		printf("%s\n", tmp->name);
		printf("%s\n", name);
		printf("strcmp : %s %s %d\n", tmp->name, name, strcmp(tmp->name, name));
		if (strcmp(tmp->name, name) == 0) return tmp->file;
	}
	return NULL;
}

struct file* parse_path(const char *path, struct file * cur_file) {
	printf("parse_path : %s\n", path);
	int len = strlen(path);
	char str[100];
	struct file * cur = root;
	int cur_pos;
	
	if (len == 1) {
		return root;
	} else if (path[0] == '/') {
		cur = root;
		cur_pos = 0;
	} else {
		assert(cur_file != NULL);
		cur = cur_file;
		cur_pos = -1;
	}
	while (cur_pos < len) {
		int nex_pos = get_next_name(path, cur_pos, len);
		memcpy(str, path + cur_pos + 1, nex_pos - cur_pos - 1);
		str[nex_pos - cur_pos - 1] = 0;
		printf("%s\n", str);
		cur = get_file(cur, str);
		if (cur == NULL) return NULL;
		cur_pos = nex_pos;
	}
	return cur;
}

struct file* parse_path2(const char *path, struct file * cur_file, char **file_name) {
	printf("parse_path2 : %s\n", path);
	int len = strlen(path);
	char str[100];
	struct file * cur = root;
	int cur_pos;
	
	if (len == 1) {
		return NULL;
	} else if (path[0] == '/') {
		cur = root;
		cur_pos = 0;
	} else {
		assert(cur_file != NULL);
		cur = cur_file;
		cur_pos = -1;
	}
	while (cur_pos < len) {
		int nex_pos = get_next_name(path, cur_pos, len);
		memcpy(str, path + cur_pos + 1, nex_pos - cur_pos - 1);
		str[nex_pos - cur_pos - 1] = 0;
		printf("!!!!!%s\n", str);
		if (path[nex_pos] == 0) {
			*file_name = (char*) malloc(sizeof(char) * (nex_pos - cur_pos + 1));
			memcpy(*file_name, path + cur_pos + 1, nex_pos - cur_pos);
			(*file_name)[nex_pos - cur_pos] = 0;
			return cur;
		}
		cur = get_file(cur, str);
		if (cur == NULL) return NULL;
		cur_pos = nex_pos;
	}
	return cur;
}


static int vexoben_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;

	struct file* file = parse_path(path, NULL);
	if (file == NULL) return -ENOENT;
	if (file->is_directory) {
		printf("%s is directory.\n", path);
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_size = 0;
	} else {
		printf("%s is not directory.\n", path);
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_size = file->size;
	}

	stbuf->st_nlink = 1;            /* Count of links, set default one link. */
	stbuf->st_uid = 0;              /* User ID, set default 0. */
	stbuf->st_gid = 0;              /* Group ID, set default 0. */
	stbuf->st_rdev = 0;             /* Device ID for special file, set default 0. */
	stbuf->st_atime = time(NULL);            /* Time of last access, set default 0. */
	stbuf->st_mtime = time(NULL);            /* Time of last modification, set default 0. */
	stbuf->st_ctime = time(NULL);            /* Time of last creation or status change, set default 0. */
	return res;
}

static int vexoben_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;

	struct file* file = parse_path(path, NULL);
	if (file == NULL) return -1;
	struct file_list * tmp;
	for (tmp = file->edges; tmp != NULL; tmp = tmp->next) {
		filler(buf, tmp->name, NULL, 0, 0);
	}

	// (void) offset;
	// (void) fi;
	// (void) flags;

	// if (strcmp(path, "/") != 0)
	// 	return -ENOENT;

	// filler(buf, ".", NULL, 0, 0);
	// filler(buf, "..", NULL, 0, 0);
	// filler(buf, options.filename, NULL, 0, 0);

	// return 0;

	return 0;
}

static int vexoben_open(const char *path, struct fuse_file_info *fi) {
	return 0;
}

static int vexoben_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	int len;
	(void) fi;

	struct file * file = parse_path(path, NULL);
	len = file->size;
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, file->data + offset, size);
	} else
		size = 0;
	memcpy(buf, file->data, len);
	return size;

	// size_t len;
	// (void) fi;
	// if(strcmp(path+1, options.filename) != 0)
	// 	return -ENOENT;

	// len = strlen(options.contents);
	// if (offset < len) {
	// 	if (offset + size > len)
	// 		size = len - offset;
	// 	memcpy(buf, options.contents + offset, size);
	// } else
	// 	size = 0;
	return size;
}

static int vexoben_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	struct file * file = parse_path(path, NULL);
	if (file == bot1 || file == bot2) {
		char * who;
		if (file == bot1) who = "bot2: ";
		else who = "bot1: ";
		char * tmp = (char*) malloc(sizeof(char) * (size + 3));
		memcpy(tmp, buf, size);
		tmp[size] = 0;
		bot1->data = realloc(bot1->data, bot1->size + size + 10);
		bot2->data = realloc(bot2->data, bot2->size + size + 10);
		bot1->data = strcat(bot1->data, who);
		bot2->data = strcat(bot2->data, who);
		bot1->data = strcat(bot1->data, tmp);
		bot2->data = strcat(bot2->data, tmp);
		bot1->data = strcat(bot1->data, "\n");
		bot2->data = strcat(bot2->data, "\n");
		bot1->size += size + 7;
		bot2->size += size + 7;
		free(tmp);
		return size;
	} else {
		file->data = realloc(file->data, size + 1);
		memcpy(file->data, buf, size);
		file->data[size] = 0;
		file->size = size;
		return size;
	}
}

static int vexoben_mknod (const char * path, mode_t mode, dev_t dev) {
	printf("vexoben mknod\n");
	struct file * nwfile = (struct file * ) malloc(sizeof(struct file));
	nwfile->is_directory = 0;
	nwfile->edges = NULL;
	nwfile->data = NULL;
	nwfile->size = 0;
	struct file * file = parse_path2(path, NULL, &(nwfile->name));
	nwfile->father = file;
	printf("father node:\n");
	printf("%s\n", file->name);
	printf("%d\n", file->is_directory);
	assert(file->is_directory == 1);
	file->edges = file_list_cons(nwfile, nwfile->name, file->edges);
	return 0;
}

static int vexoben_utimens (const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
	printf("vexoben_utimens\n");
	return 0;
}

static int vexoben_mkdir (const char *path, mode_t mode) {
	struct file * nwfile = (struct file * ) malloc(sizeof(struct file));
	nwfile->is_directory = 1;
	nwfile->edges = NULL;
	nwfile->data = NULL;
	nwfile->size = 0;
	struct file * file = parse_path2(path, NULL, &(nwfile->name));
	nwfile->father = file;
	assert(file->is_directory == 1);
	nwfile->edges = init_file_edges(file, nwfile);
	file->edges = file_list_cons(nwfile, nwfile->name, file->edges);
	return 0;
}

void remove_link(char * name, struct file_list ** list) {
	struct file_list * t1, *t2;
	t1 = NULL; t2 = *list;
	while (t2 != NULL && strcmp(name, t2->name) != 0) {
		t1 = t2;
		t2 = t2->next;
	}
	assert(t2 != NULL);
	if (t1 == NULL) {
		*list = t2->next;
	} else {
		t1->next = t2->next;
	}
}

static int vexoben_unlink (const char *path) {
	struct file * file = parse_path(path, NULL);
	if (file == NULL) return -ENOENT;
	else if (file->is_directory) return -EISDIR;
	else {
		struct file * father = file->father;
		remove_link(file->name, &(father->edges));
		free(file);
		return 0;
	}
}

void delete_dir(struct file * file) {
	if (!file->is_directory) free(file);
	else {
		struct file_list * list = file->edges;
		while (list != NULL) {
			if (strcmp(list->name, "..") != 0 && strcmp(list->name, ".") != 0) {
				delete_dir(list->file);
			}
			list = list->next;
		}
		free(file);
	}
}

static int vexoben_rmdir (const char *path) {
	struct file * file = parse_path(path, NULL);
	if (file == NULL) return -ENOENT;
	else if (!(file->is_directory)) return -ENOTDIR;
	else {
		struct file * father = file->father;
		remove_link(file->name, &(father->edges));
		delete_dir(file);
		return 0;
	}
}

static const struct fuse_operations vexoben_oper = {
	.init    = vexoben_init,
	.getattr	= vexoben_getattr,
	.readdir	= vexoben_readdir,
	.open		= vexoben_open,
	.read		= vexoben_read,
	.mknod   = vexoben_mknod,
	.utimens = vexoben_utimens,
	.write   = vexoben_write,
	.mkdir   = vexoben_mkdir,
	.unlink  = vexoben_unlink,
	.rmdir   = vexoben_rmdir,
};

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --name=<s>          Name of the \"vexoben\" file\n"
	       "                        (default: \"vexoben\")\n"
	       "    --contents=<s>      Contents \"vexoben\" file\n"
	       "                        (default \"vexoben, World!\\n\")\n"
	       "\n");
}

int main(int argc, char *argv[])
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/* Set defaults -- we have to use strdup so that
	   fuse_opt_parse can free the defaults if other
	   values are specified */
	options.filename = strdup("vexoben");
	options.contents = strdup("vexoben World!\n");

	/* Parse options */
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string) */
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}

	ret = fuse_main(args.argc, args.argv, &vexoben_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
