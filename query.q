//This file was generated from (Commercial) UPPAAL 4.0.14 (rev. 5615), May 2014

/*

*/
A.VReceived && cV > LRI[paceMode] --> alarmSlow

/*

*/
A.VReceived && cV < URI[paceMode] --> alarmFast

/*

*/
A[] (L.BlinkAS imply ASC.c == 0)

/*

*/
A[] (L.BlinkVS imply VSC.c == 0)

/*

*/
A[] (L.BlinkAP imply cA == 0)

/*

*/
A[] (L.BlinkVP imply cV == 0)

/*

*/
A[] P.APSend imply (cV == LRI[paceMode] - AVI_min) and (cV >= PVARP)

/*

*/
A[] P.VPSend imply (cALast == AVI_max and cALast > AVI_min) or (cVLast == LRI[paceMode] and cVLast > URI[paceMode] and cVLast > VRP)

/*

*/
A[] not fail

/*

*/
A[] not deadlock
