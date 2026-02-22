import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 240,
      width: 640,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 1, 36, 1),
            height: 240,
            width: 640,
            child: Stack(
              children: [
                Positioned(
                  height: 56,
                  left: 30,
                  top: 29,
                  width: 120,
                  child: Text(
                    "12pt", 
                    textAlign: TextAlign.center,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 255, 255, 255),
                      fontFamily: "Source Han Sans",
                      fontSize: 50,
                      fontVariations: [FontVariation.weight(600)],
                      height: 1,
                    ),
                  ),
                ),
                Positioned(
                  height: 168,
                  left: 150,
                  top: 56,
                  width: 478,
                  child: Text(
                    "150px", 
                    textAlign: TextAlign.center,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 255, 255, 255),
                      fontFamily: "Source Han Sans",
                      fontSize: 150,
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
