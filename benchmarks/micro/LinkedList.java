import java.lang.System;

class LinkedList {
    LinkedList reames_next = null;

    public static void main(String [ ] args) {
        LinkedList head = new LinkedList();
        for(int i = 0; i < 10000; i++) {
            LinkedList temp = new LinkedList();
            temp.reames_next = head;
            head = temp;
        }
        System.gc();
    }
}
