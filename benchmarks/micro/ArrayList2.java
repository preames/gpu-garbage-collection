import java.lang.System;
import java.util.ArrayList;

class ArrayList2 {
    static class Inner {
        Inner(String p) {
            s=p;
        }
        String s;
    }
  

    public static void main(String [ ] args) {
        System.out.println("Starting...");
        java.util.ArrayList<Inner> list = new java.util.ArrayList<Inner>();
        for(int i = 0; i < 20000; i++) {
            list.add(new Inner("foo"));
        }
        System.gc();
    }
}
