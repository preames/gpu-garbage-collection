import java.lang.System;

class BigArray {
    static class Inner {
        String s;
    }

    public static void main(String [ ] args) {
        Inner[] arr = new Inner[20000];
        for(int i = 0; i < 20000; i++) {
            arr[i] = new Inner();
            arr[i].s = "foo";
        }
        System.gc();
    }
}
