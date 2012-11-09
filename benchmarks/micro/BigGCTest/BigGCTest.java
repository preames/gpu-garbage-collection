public class BigGCTest {
    public static int LEVELS = 100;
    public static int NODES_PER_LEVEL = 1024;
    public static int LINKS_PER_NODE = 1;

    private static Node[][] nodes;

    public static void main(String[] args) {
	System.out.println("Running..");
	nodes = new Node[LEVELS][NODES_PER_LEVEL];

	for (int i = 0; i < LEVELS; i++) {
	    for (int j = 0; j < NODES_PER_LEVEL; j++) {
		nodes[i][j] = new Node(LINKS_PER_NODE);

		if (i > 0) {
		    for (int k = 0; k < LINKS_PER_NODE; k++) {
			int pred_index = j-(LINKS_PER_NODE/2)+k;
			if (pred_index >= 0 && pred_index < NODES_PER_LEVEL)
			    nodes[i-1][pred_index].succ[k] = nodes[i][j];
		    }
		}
	    }

	    if (i > 1) {
		for (int j = 0; j < NODES_PER_LEVEL; j++) {
		    nodes[i-1][j] = null;
		}
	    }
	}

	System.gc();
    }
}