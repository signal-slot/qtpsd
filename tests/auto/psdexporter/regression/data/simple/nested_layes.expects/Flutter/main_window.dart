import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 240,
      width: 320,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 247, 231, 144),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Container(
                  child: Stack(
                    children: [
                      Positioned(
                        height: 52,
                        left: 39,
                        top: 49,
                        width: 182,
                        child: Container(
                          decoration: BoxDecoration(
                            borderRadius: BorderRadius.circular(20),
                            color: Color.fromARGB(255, 212, 245, 201),
                          ),
                        ),
                      ),
                      Container(
                        child: Stack(
                          children: [
                            Positioned(
                              height: 13,
                              left: 72,
                              top: 89,
                              width: 139,
                              child: Container(
                                decoration: BoxDecoration(
                                  color: Color.fromARGB(255, 33, 121, 3),
                                ),
                              ),
                            ),
                            Positioned(
                              height: 14,
                              left: 79,
                              top: 81,
                              width: 25,
                              child: Container(
                                decoration: BoxDecoration(
                                  color: Color.fromARGB(255, 94, 136, 231),
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),
                      Positioned(
                        height: 33,
                        left: 50,
                        top: 63,
                        width: 160,
                        child: Text(
                          "Example1", 
                          textAlign: TextAlign.center,
                          textScaler: TextScaler.linear(1),
                          style: TextStyle(
                            color: Color.fromARGB(255, 0, 0, 0),
                            fontFamily: "Source Han Sans",
                            fontSize: 30,
                            fontVariations: [FontVariation.weight(600)],
                            height: 1,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                Container(
                  child: Stack(
                    children: [
                      Positioned(
                        height: 52,
                        left: 39,
                        top: 134,
                        width: 182,
                        child: Container(
                          decoration: BoxDecoration(
                            borderRadius: BorderRadius.circular(20),
                            color: Color.fromARGB(255, 212, 245, 201),
                          ),
                        ),
                      ),
                      Container(
                        child: Stack(
                          children: [
                            Positioned(
                              height: 12,
                              left: 72,
                              top: 174,
                              width: 139,
                              child: Container(
                                decoration: BoxDecoration(
                                  color: Color.fromARGB(255, 33, 121, 3),
                                ),
                              ),
                            ),
                            Positioned(
                              height: 14,
                              left: 179,
                              top: 166,
                              width: 25,
                              child: Container(
                                decoration: BoxDecoration(
                                  color: Color.fromARGB(255, 94, 136, 231),
                                ),
                              ),
                            ),
                          ],
                        ),
                      ),
                      Positioned(
                        height: 33,
                        left: 50,
                        top: 143,
                        width: 160,
                        child: Text(
                          "Example1", 
                          textAlign: TextAlign.center,
                          textScaler: TextScaler.linear(1),
                          style: TextStyle(
                            color: Color.fromARGB(255, 0, 0, 0),
                            fontFamily: "Source Han Sans",
                            fontSize: 30,
                            fontVariations: [FontVariation.weight(600)],
                            height: 1,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
