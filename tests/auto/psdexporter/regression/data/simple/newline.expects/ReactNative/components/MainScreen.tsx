import React from 'react';
import { View, StyleSheet, Text } from 'react-native';

const MainScreen: React.FC = () => {
  return (
    <View style={styles.root}>
      <View style={styles.style0}>
        <Text style={styles.style1}></Text>
        <Text style={styles.style2}>文字列中に\n改行</Text>
        <View style={styles.style3}>
          <Text style={styles.style4}>文字列</Text>
          <Text style={styles.style5}>中</Text>
          <Text style={styles.style6}>に</Text>
          <Text style={styles.style7}>別</Text>
          <Text style={styles.style8}>フォント</Text>
        </View>
        <Text style={styles.style9}>Shift\n+改行</Text>
        <Text style={styles.style10}>段落テキストは折り返される</Text>
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
    backgroundColor: '#012401',
  },
  style1: {
    position: 'absolute',
    left: 0,
    top: 0,
    fontFamily: 'Source Han Sans',
    fontSize: 50,
    color: '#EAC3C3',
    textAlign: 'center',
  },
  style10: {
    position: 'absolute',
    left: 160,
    top: 110,
    width: 158,
    height: 116,
    fontFamily: 'Koz Go Pr6N',
    fontSize: 24,
    color: '#EAC3C3',
    textAlign: 'center',
  },
  style2: {
    position: 'absolute',
    left: 2,
    top: 10,
    width: 160,
    height: 110,
    fontFamily: 'Source Han Sans',
    fontSize: 30,
    color: '#EAC3C3',
    textAlign: 'center',
  },
  style3: {
    position: 'absolute',
    left: 169,
    top: 10,
    width: 144,
    height: 98,
  },
  style4: {
    fontFamily: 'Koz Go Pr6N',
    fontSize: 30,
    color: '#EAC3C3',
  },
  style5: {
    fontFamily: 'Koz Go Pr6N',
    fontSize: 16,
    color: '#EAC3C3',
  },
  style6: {
    fontFamily: 'Koz Go Pr6N',
    fontSize: 30,
    color: '#EAC3C3',
  },
  style7: {
    fontFamily: 'Koz Go Pr6N',
    fontSize: 30,
    color: '#EAC3C3',
  },
  style8: {
    fontFamily: 'Source Han Sans',
    fontSize: 18,
    color: '#EAC3C3',
  },
  style9: {
    position: 'absolute',
    left: 25,
    top: 110,
    width: 86,
    height: 89,
    fontFamily: 'Koz Go Pr6N',
    fontSize: 30,
    color: '#EAC3C3',
    textAlign: 'center',
  },
});

export default MainScreen;
