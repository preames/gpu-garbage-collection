import java.util.ArrayList;


class Turnover {
    static class Inner {
        Inner(String p) {
            s=p;
        }
        String s;
    }
  

    public static void main(String [ ] args) {
        System.out.println("Starting...");
        java.util.ArrayList<Inner> list = new java.util.ArrayList<Inner>();
        for(int i = 0; i < 200000; i++) {
            list.add(new Inner("foo"));
            if( i % 1000 == 0) {
                list = new java.util.ArrayList<Inner>();
            }
        }
    }
}
