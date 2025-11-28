#include "logger.h"
#include "config.h"
#include "module/tf.h"

#include "FS.h"
#include "SD_MMC.h"

void tf::setup()
{
  logger::debugln("TF card is starting...");

  if (TF_1BIT_MODE)
  {
    if (!SD_MMC.setPins(TF_CLK_IO, TF_CMD_IO, TF_D0_IO))
    { // 1-bit line version
      logger::debugln("TF card pin change failed!");
      return;
    }
  }
  else if (!SD_MMC.setPins(TF_CLK_IO, TF_CMD_IO, TF_D0_IO, TF_D1_IO, TF_D2_IO, TF_D3_IO))
  { // 4-bit line version
    logger::debugln("TF card pin change failed!");
    return;
  }

  // 检测TF卡是否插入
  if (SD_MMC.begin("/card", TF_1BIT_MODE, false, TF_FREQ))
  {
    logger::debugln("TF card is mounted.");
    switch (SD_MMC.cardType())
    {
    case CARD_MMC:
    {
      logger::debugln("TF card type is MMC.");
      break;
    }
    case CARD_SD:
    {
      logger::debugln("TF card type is SD.");
      break;
    }
    case CARD_SDHC:
    {
      logger::debugln("TF card type is SDHC.");
      break;
    }
    default:
    {
      logger::warnln("TF card type is error!");
      break;
    }
    }
    logger::debugln("TF card Size: %dMB\n", SD_MMC.cardSize() / (1024 * 1024));
  }

  logger::debugln("TF card is started.");
}