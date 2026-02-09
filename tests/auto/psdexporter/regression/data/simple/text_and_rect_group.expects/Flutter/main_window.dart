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
            color: Color.fromARGB(255, 93, 82, 6),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Container(
                  child: Stack(
                    children: [
                      Positioned(
                        height: 62,
                        left: 48,
                        top: 79,
                        width: 163,
                        child: Container(
                          decoration: BoxDecoration(
                            color: Color.fromARGB(255, 245, 245, 237),
                          ),
                        ),
                      ),
                      Positioned(
                        height: 34,
                        left: 52,
                        top: 80,
                        width: 157,
                        child: Container(
                          decoration: BoxDecoration(
                            color: Color.fromARGB(255, 247, 231, 144),
                          ),
                        ),
                      ),
                      Positioned(
                        height: 58,
                        left: 50,
                        top: 80,
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
