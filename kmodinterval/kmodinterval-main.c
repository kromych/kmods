#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interval_tree.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kromych");
MODULE_DESCRIPTION("Parsing interval list");
MODULE_VERSION("0.01");

static char* interval_list_str;

module_param(interval_list_str, charp, 0644); // Permissions in /sysfs
MODULE_PARM_DESC(interval_list_str, "List of intervals");

static struct rb_root_cached intervals = RB_ROOT_CACHED;

enum interval_parse_outcome {
    VALID_INTERVAL_LIST,
    EXPECTED_INTERVAL_START,
    EXPECTED_INTERVAL_END,
    UNEXPECTED_CHAR,
    INVALID_INTERVAL,
    VALID_INTERVAL,
    OVERLAPPING_INTERVAL
};

typedef enum interval_parse_outcome (*interval_parse_callback)(void* semantic_state,
                        u64 start, u64 end);

enum interval_parse_outcome
interval_list_parse(char* interval_list, void* semantic_state,
    interval_parse_callback semantic_cb)
{
    enum interval_parse_outcome result = VALID_INTERVAL_LIST;
    char* curr = interval_list;
    char* next = interval_list;
    u64 start;
    u64 end;

    for (;;) {
        start = simple_strtoull(curr, &next, 0);
        if (next == curr) {
            return EXPECTED_INTERVAL_START;
        }
        curr = next;

        if (*curr == '-') {
            ++curr;
            end = simple_strtoull(curr, &next, 0);
            if (next == curr) {
                return EXPECTED_INTERVAL_END;
            }
            curr = next;
        } else
            end = start;

        if (start > end)
            return INVALID_INTERVAL;

        if (semantic_cb) {
            result = semantic_cb(semantic_state, start, end);
            if (result == VALID_INTERVAL)
                result = VALID_INTERVAL_LIST;
            else
                return result;
        }

        if (*curr != ',') {
            if (*curr == '\x0')
                break;
            else
                return UNEXPECTED_CHAR;
        } else
            ++curr;
    }

    return result;
}

enum interval_parse_outcome
interval_insert(void* semantic_state, u64 start, u64 end)
{
    struct interval_tree_node* node = NULL;

    node = interval_tree_iter_first(&intervals, start, end);
    if (node)
        return OVERLAPPING_INTERVAL;
    node = kzalloc(sizeof(struct interval_tree_node), GFP_KERNEL);
    if (!node)
        return -ENOMEM;

    node->start = start;
    node->last = end;
    interval_tree_insert(node, &intervals);

    return VALID_INTERVAL;
}

// sudo insmod kmodinterval.ko interval_list=2,3,45-0xFFFF
static int __init init_intervals(void)
{
    enum interval_parse_outcome parse_outcome;

    if (!interval_list_str) {
        pr_err("No interval list provided!\n");
        return -ENOPARAM;
    }
    pr_info("Interval list passed: %s\n", interval_list_str);

    parse_outcome = interval_list_parse(interval_list_str, &intervals, interval_insert);
    switch (parse_outcome)
    {
        case VALID_INTERVAL_LIST:
            pr_info("Parsed the interval list\n");
            break;
        case EXPECTED_INTERVAL_START:
            pr_err("Couldn't parse the interval list: error EXPECTED_INTERVAL_START\n");
            return -EBADCOOKIE;
        case EXPECTED_INTERVAL_END:
            pr_err("Couldn't parse the interval list: error EXPECTED_INTERVAL_END\n");
            return -EBADCOOKIE;
        case UNEXPECTED_CHAR:
            pr_err("Couldn't parse the interval list: error UNEXPECTED_CHAR\n");
            return -EBADCOOKIE;
        case INVALID_INTERVAL:
            pr_err("Couldn't parse the interval list: error INVALID_INTERVAL\n");
            return -EBADCOOKIE;
        case OVERLAPPING_INTERVAL:
            pr_err("Couldn't parse the interval list: error OVERLAPPING_INTERVAL\n");
            return -EBADCOOKIE;
        default:
            pr_err("Couldn't parse the interval list: error %#x\n", parse_outcome);
            return -EBADCOOKIE;
    }

    return 0;
}

static void __exit exit_intervals(void)
{
}

module_init(init_intervals);
module_exit(exit_intervals);
