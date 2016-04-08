#!/bin/bash -x

javac -source 6 -target 6 classpath/pmath/*.java
javac -source 6 -target 6 classpath/pmath/util/*.java

javac -source 6 -target 6 -cp classpath example/App/*.java
javac -source 6 -target 6 -cp classpath example/Interrupt/*.java
