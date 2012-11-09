public class Node {
	public Node left;
	public Node right;
	public int[] a;

	public Node(int n) {
		a = new int[100];
		if (n != 0) {
			left = new Node(n-1);
			right = new Node(n-1);
		}
	}
}
