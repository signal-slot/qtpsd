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
            color: Color.fromARGB(255, 27, 134, 138),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Text(
                  "", 
                  textAlign: TextAlign.center,
                  textScaler: TextScaler.linear(1),
                  style: TextStyle(
                    color: Color.fromARGB(255, 0, 0, 0),
                    fontFamily: "Source Han Sans",
                    fontSize: 50,
                    fontVariations: [FontVariation.weight(600)],
                    height: 1,
                  ),
                ),
                Positioned(
                  height: 100,
                  left: 29,
                  top: 19,
                  width: 242,
                  child: Container(
                    decoration: BoxDecoration(
                      color: Color.fromARGB(255, 168, 246, 237),
                    ),
                  ),
                ),
                Positioned(
                  height: 52,
                  left: 29,
                  top: 19,
                  width: 242,
                  child: Container(
                    decoration: BoxDecoration(
                      color: Color.fromARGB(255, 127, 100, 246),
                    ),
                  ),
                ),
                Positioned(
                  height: 56,
                  left: 30,
                  top: 19,
                  width: 239,
                  child: Text(
                    "shooting", 
                    textAlign: TextAlign.center,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 0, 0, 0),
                      fontFamily: "Source Han Sans",
                      fontSize: 50,
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
    );
  }
}
