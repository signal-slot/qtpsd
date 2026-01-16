import React from 'react';
import { View, StyleSheet, Text } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Text style={styles.style1}>Example1</Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  root: {
    flex: 1,
    width: 320,
    height: 240,
    backgroundColor: '#FFFFFF',
  },
  style0: {
    position: 'absolute',
    left: 0,
    top: 0,
    backgroundColor: '#F7E790',
  },
  style1: {
    position: 'absolute',
    left: 50,
    top: 80,
    width: 160,
    height: 43,
    fontFamily: '源ノ角ゴシック JP',
    fontSize: 30,
    color: '#000000',
    textAlign: 'center',
  },
});

export default MainScreen;
