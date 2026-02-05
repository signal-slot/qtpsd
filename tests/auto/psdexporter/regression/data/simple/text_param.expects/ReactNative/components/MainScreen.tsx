import React from 'react';
import { View, StyleSheet, Text } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Text style={styles.style1}></Text>
        <View style={styles.style2} />
        <View style={styles.style3} />
        <Text style={styles.style4}>shooting</Text>
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
    backgroundColor: '#1B868A',
  },
  style1: {
    position: 'absolute',
    left: 0,
    top: 0,
    fontFamily: 'SourceHanSans-Medium',
    fontSize: 50,
    color: '#000000',
    textAlign: 'center',
  },
  style2: {
    position: 'absolute',
    left: 29,
    top: 19,
    width: 242,
    height: 100,
    backgroundColor: '#A8F6ED',
  },
  style3: {
    position: 'absolute',
    left: 29,
    top: 19,
    width: 242,
    height: 52,
    backgroundColor: '#7F64F6',
  },
  style4: {
    position: 'absolute',
    left: 30,
    top: 20,
    width: 239,
    height: 54,
    fontFamily: 'SourceHanSans-Medium',
    fontSize: 50,
    color: '#000000',
    textAlign: 'center',
  },
});

export default MainScreen;
