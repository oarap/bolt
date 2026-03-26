import sys
import xmlrpc.client
import subprocess
import threading
import time

def run_spark():
    # Write the scala script for sparking
    with open('/home/omer.arap/bolt/spark_query.scala', 'w') as f:
        f.write("""
import org.apache.spark.sql.functions._
import org.apache.spark.sql.types._
import scala.util.Random

// Create a massive dataframe that will take a long time to process
// and stall the CPU
val df = spark.range(0, 500000000, 1, 1).withColumn("v1", rand() * 100).withColumn("v2", rand() * 1000)
df.createOrReplaceTempView("large_table")

// Run a heavy grouping operation 
spark.sql("SELECT cast(v1 as int) as id, sum(v2), count(*), avg(v2) FROM large_table GROUP BY cast(v1 as int) ORDER BY sum(v2) DESC").show()
""")
        
    cmd = "source ~/.bash_profile && sparkshell -i /home/omer.arap/bolt/spark_query.scala"
    
    print("Executing Spark query...")
    subprocess.run(cmd, shell=True, executable='/bin/bash')

if __name__ == "__main__":
    run_spark()
