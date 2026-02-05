import React from 'react';
import { View, StyleSheet, Text } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <View style={styles.style1}>
          <View style={styles.style2} />
          <View style={styles.style3} />
          <Text style={styles.style4}>Example1</Text>
        </View>
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
    backgroundColor: '#5D5206',
  },
  style1: {
    position: 'absolute',
    left: 0,
    top: 0,
  },
  style2: {
    position: 'absolute',
    left: 48,
    top: 79,
    width: 163,
    height: 62,
    backgroundColor: '#F5F5ED',
  },
  style3: {
    position: 'absolute',
    left: 52,
    top: 80,
    width: 157,
    height: 34,
    backgroundColor: '#F7E790',
  },
  style4: {
    position: 'absolute',
    left: 50,
    top: 80,
    width: 160,
    height: 33,
    fontFamily: 'SourceHanSans-Medium',
    fontSize: 30,
    color: '#000000',
    textAlign: 'center',
  },
});

export default MainScreen;
