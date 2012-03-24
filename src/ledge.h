/* ledge.h
 */

#include "local-trees.h"
#include "tree.h"

typedef unsigned short	stringpos;
#define STRINGPOSMAX	USHRT_MAX

struct ledge {
  stringpos	left, right;
  si_index	label;
};

struct ledges {
  size_t	n;
  struct ledge	*le;
};

void free_ledges(struct ledges *les);
struct ledges *tree_ledges(tree t);
size_t common_ledge_count(struct ledges *les1, struct ledges *les2);

