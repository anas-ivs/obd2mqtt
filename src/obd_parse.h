#pragma once
// Platform-independent (also testable on a PC): parse ELM327 response + evaluate formula
#include <string>
#include <vector>
#include <cstdint>
#include <utility>

namespace ObdParse {

enum ErrKind { ERR_NONE, ERR_NO_DATA, ERR_INCOMPLETE, ERR_CAN, ERR_NRC, ERR_OTHER };
extern ErrKind lastErrKind;          // kind of the last error from parseResponse()

// Expected response prefix: 1st byte + 0x40, rest unchanged ("220105" → "620105", "015B" → "415B")
std::string expectedPrefix(const std::string& cmd);

// Converts the raw ELM response (ATH0, ATCAF1, ATS0 or ATS1) to bytes.
// Supports single-frame, multi-frame ("03E / 0: .. / 1: ..") and multiple responding ECUs.
// bytes starts with the service byte (e.g. 0x62).
bool parseResponse(const std::string& raw, const std::string& cmd,
                   std::vector<uint8_t>& bytes, std::string& err);

// Formula with variables B0..Bn and functions u16(hi,lo), s16(hi,lo), s8(x), bit(x,n)
// Battery values of the active profile for formulas: CAP = usable capacity (kWh), CONS = consumption (kWh/100 km)
void setBattery(double capKwh, double consKwh100);
void getBattery(double& capKwh, double& consKwh100);
// Last values of the other PIDs as variables (name = PID ID), e.g. "-I*hv_voltage/1000"
void setValues(const std::vector<std::pair<std::string, double>>& v);
bool evalFormula(const std::string& formula, const std::vector<uint8_t>& bytes,
                 double& value, std::string& err);

std::string toHex(const std::vector<uint8_t>& bytes, bool spaces = true);

}  // namespace ObdParse
