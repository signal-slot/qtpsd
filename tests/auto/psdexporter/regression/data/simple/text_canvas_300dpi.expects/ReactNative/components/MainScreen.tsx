import React from 'react';
import { View, StyleSheet, Text } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Text style={styles.style1}>12pt</Text>
        <Text style={styles.style2}>150px</Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: {
    flex: 1,
    width: 640,
    height: 240,
    backgroundColor: '#FFFFFF',
  },
  style0: {
    position: 'absolute',
    left: 0,
    top: 0,
    backgroundColor: '#012401',
  },
  style1: {
    position: 'absolute',
    left: 30,
    top: 30,
    width: 120,
    height: 70,
    fontFamily: '源ノ角ゴシック JP',
    fontSize: 50,
    color: '#FFFFFF',
    textAlign: 'center',
  },
  style2: {
    position: 'absolute',
    left: 150,
    top: 60,
    width: 478,
    height: 211,
    fontFamily: '源ノ角ゴシック JP',
    fontSize: 150,
    color: '#FFFFFF',
    textAlign: 'center',
  },
});

export default MainScreen;
