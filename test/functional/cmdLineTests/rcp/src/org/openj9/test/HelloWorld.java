package org.openj9.test;

import java.util.EnumSet;
enum Day {
    MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY
}

class HelloWorld {
    static EnumSet<Day> weekend = EnumSet.of(Day.SATURDAY, Day.SUNDAY);
    String say = new String("Hello World");

    public static void main(String[] args)
    {
	HelloWorld hi = new HelloWorld();
        System.out.println(hi.say + weekend);
    }
}
