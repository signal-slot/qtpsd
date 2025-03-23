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
            color: Color.fromARGB(255, 1, 36, 1),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Column(
                  children: [
                    Text(
                      "", 
                      textScaler: TextScaler.linear(1),
                      style: TextStyle(
                        color: Color.fromARGB(255, 234, 195, 195),
                        fontFamily: "源ノ角ゴシック JP",
                        fontSize: 50,
                        fontVariations: [FontVariation.weight(600)],
                        height: 1,
                      ),
                    ),
                  ],
                ),
                Positioned(
                  height: 110,
                  left: 2,
                  top: 10,
                  width: 160,
                  child: Column(
                    children: [
                      Text(
                        "文字列中に", 
                        textScaler: TextScaler.linear(1),
                        style: TextStyle(
                          color: Color.fromARGB(255, 234, 195, 195),
                          fontFamily: "源ノ角ゴシック JP",
                          fontSize: 30,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                      Text(
                        "改行", 
                        textScaler: TextScaler.linear(1),
                        style: TextStyle(
                          color: Color.fromARGB(255, 234, 195, 195),
                          fontFamily: "源ノ角ゴシック JP",
                          fontSize: 30,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 98,
                  left: 169,
                  top: 10,
                  width: 144,
                  child: Column(
                    children: [
                      Row(
                        children: [
                          Text(
                            "文字列", 
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 30,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                          Text(
                            "中", 
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 16,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                          Text(
                            "に", 
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 30,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                        ],
                      ),
                      Row(
                        children: [
                          Text(
                            "別", 
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 30,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                          Text(
                            "フォント", 
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "源ノ角ゴシック JP",
                              fontSize: 18,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 89,
                  left: 25,
                  top: 110,
                  width: 86,
                  child: Column(
                    children: [
                      Text(
                        "Shift", 
                        textScaler: TextScaler.linear(1),
                        style: TextStyle(
                          color: Color.fromARGB(255, 234, 195, 195),
                          fontFamily: "KozGoPr6N-Regular",
                          fontSize: 30,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                      Text(
                        "+改行", 
                        textScaler: TextScaler.linear(1),
                        style: TextStyle(
                          color: Color.fromARGB(255, 234, 195, 195),
                          fontFamily: "KozGoPr6N-Regular",
                          fontSize: 30,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 116,
                  left: 160,
                  top: 110,
                  width: 158,
                  child: Column(
                    children: [
                      Text(
                        "段落テキストは折り返される", 
                        textScaler: TextScaler.linear(1),
                        style: TextStyle(
                          color: Color.fromARGB(255, 234, 195, 195),
                          fontFamily: "KozGoPr6N-Regular",
                          fontSize: 24,
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
