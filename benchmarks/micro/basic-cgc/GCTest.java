public class GCTest {
	private static int[] a;
	private static Node n;

	public static void main(String[] args) {
		a = new int[1000000];
		n = new Node(10);
		System.gc();
	}
}
