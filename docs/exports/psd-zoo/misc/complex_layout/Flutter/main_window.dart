import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 400,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 400,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 400,
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 300,
                  left: 0,
                  top: 0,
                  width: 400,
                  child: Image.asset(
                    "assets/images/bg_color.png", 
                    fit: BoxFit.contain,
                    height: 300,
                    width: 400,
                  ),
                ),
              ],
            ),
          ),
          Container(
            child: Stack(
              children: [
                Positioned(
                  height: 242,
                  left: 49,
                  top: 29,
                  width: 302,
                  child: Container(
                    decoration: BoxDecoration(
                      color: Color.fromARGB(255, 255, 255, 255),
                    ),
                  ),
                ),
                Positioned(
                  height: 26,
                  left: 80,
                  top: 59,
                  width: 87,
                  child: Text(
                    "Card Title", 
                    textAlign: TextAlign.left,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 30, 30, 30),
                      fontFamily: "Roboto",
                      fontSize: 20,
                      fontVariations: [FontVariation.weight(600)],
                      height: 1,
                    ),
                  ),
                ),
                Positioned(
                  height: 121,
                  left: 80,
                  top: 109,
                  width: 250,
                  child: Text(
                    "This is body text in a card layout with multiple layers and groups.", 
                    textAlign: TextAlign.left,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 80, 80, 80),
                      fontFamily: "Roboto",
                      fontSize: 12,
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
