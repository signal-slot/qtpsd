import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 300,
            ),
          ),
          Positioned(
            height: 36,
            left: 30,
            top: 72,
            width: 203,
            child: Column(
              children: [
                Row(
                  children: [
                    Text(
                      "😀", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Color Emoji",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                    Text(
                      "", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Sans",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                    Text(
                      "🚀", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Color Emoji",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                    Text(
                      "", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Sans",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                    Text(
                      "❤", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Color Emoji",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                    Text(
                      "", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Sans",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                    Text(
                      "🌟", 
                      textAlign: TextAlign.left,
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 0, 0, 0),
                        fontFamily: "Noto Color Emoji",
                        fontSize: 36,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
