# scrapy-sebug

ʹ��scrapy�����ȡsebug©�������ݣ�������mysql���ݿ⡣

��̫�˽�scrapy��ͯЬ�����http://doc.scrapy.org/en/0.24/intro/overview.html

==========================

��1���򵥶���sebug��©������ҳ��item���ݽṹ��

class SebugItem(scrapy.Item):
    # define the fields for your item here like:
    # name = scrapy.Field()
    ssv = Field()
    appdir = Field()
    title = Field()
    content = Field()
    publishdate = Field()
    
��2��mysql���ݿⴴ����Ӧ�ı�ṹ

��3��Ϊ��ֹ���汻ban,����setting.py

DOWNLOAD_DELAY = 2
RANDOMIZE_DOWNLOAD_DELAY = True
USER_AGENT = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_8_3) AppleWebKit/536.5 (KHTML, like Gecko) Chrome/19.0.1084.54 Safari/536.5'

��4����ʼ��������

scrapy crawl sebugvul