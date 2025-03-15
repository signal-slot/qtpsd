import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 100,
      width: 640,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 188, 245, 188),
            height: 100,
            width: 640,
            child: Stack(
              children: [
                Positioned(
                  height: 96,
                  left: 20,
                  top: 20,
                  width: 99,
                  child: Column(
                    children: [
                      Text(
                        "hug", 
                        style: TextStyle(
                          color: Color.fromARGB(255, 0, 0, 0),
                          fontFamily: "SourceHanSans-Medium",
                          fontSize: 33.3333,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 57,
                  left: 120,
                  top: 20,
                  width: 87,
                  child: Column(
                    children: [
                      Text(
                        "hug", 
                        style: TextStyle(
                          color: Color.fromARGB(255, 0, 0, 0),
                          fontFamily: "TimesNewRomanPSMT",
                          fontSize: 33.3333,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 54,
                  left: 420,
                  top: 20,
                  width: 154,
                  child: Column(
                    children: [
                      Text(
                        "hub", 
                        style: TextStyle(
                          color: Color.fromARGB(255, 0, 0, 0),
                          fontFamily: "Wingdings",
                          fontSize: 33.3333,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 50,
                  left: 210,
                  top: 21,
                  width: 82,
                  child: Column(
                    children: [
                      Text(
                        "hug", 
                        style: TextStyle(
                          color: Color.fromARGB(255, 0, 0, 0),
                          fontFamily: "MS PMincho",
                          fontSize: 33.3333,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 56,
                  left: 310,
                  top: 20,
                  width: 91,
                  child: Column(
                    children: [
                      Text(
                        "hug", 
                        style: TextStyle(
                          color: Color.fromARGB(255, 0, 0, 0),
                          fontFamily: "Exo2Roman-Thin",
                          fontSize: 33.3333,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
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
