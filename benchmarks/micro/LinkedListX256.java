import java.lang.System;

class LinkedListX256 {
    
    public static void main(String [ ] args) {
        /*scope*/ {
            LinkedList[] arr = new LinkedList[256];
            for(int i = 0; i < arr.length; i++) {
                LinkedList head = new LinkedList();
                arr[i] = head;
                for(int j = 0; j < 10000; j++) {
                    LinkedList temp = new LinkedList();
                    temp.reames_next = head;
                    head = temp;
                }
            }
            System.gc();
            System.out.println(arr.length);
        }
        System.gc();
    }
}
