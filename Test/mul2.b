/* This is an example of how moving constants into a variable and
 * thereby loading them only once can improve the speed of bc.
 */
scale = 20
c0=0
c1=0
c2=100
c11=10000
for (i=c0; i<c11; i++) {
  for (j=c1; j<c2; j++) b=i*j
}
b
c12=1000000000000000000000000000000000000000000000000000000000000000000
c13=1000000000000000000000000000000000000000000000000000000000000000100
for (i=c0; i<c11; i++) {
  for (j=c12; j<c13; j++) b=i*j
}
b
quit
